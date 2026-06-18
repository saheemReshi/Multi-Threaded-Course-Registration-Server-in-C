// imt2023051

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>

#include "common.h" 
#include "utils.h"	

#define LINE_SIZE_MAX 1000
#define LIST_SIZE_MAX 1000
#define FIELD_SIZE_MAX 200

#define STUDENT_FILE "data/students.txt"
#define FACULTY_FILE "data/faculty.txt"
#define COURSE_FILE  "data/courses.txt"


int server_fd = -1; // global so we can access in signal handler

void handle_sigint(int sig) {
    printf("\n[Server] Caught signal %d, shutting down.\n", sig);
    if (server_fd >= 0)
        close(server_fd);
    // If needed: clean up temp files, shared memory, etc.
    exit(0);
}


// Load the contents of a text file and split into lines with locking
static int load_file(const char *path, File *file_struct, int lock_mode)
{
	int fd = open(path, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		return -1;

	if (fileLock(fd, lock_mode) < 0)
		return -1;

	struct stat meta;
	if (fstat(fd, &meta) < 0)
		return -1;

	size_t file_size = (size_t)meta.st_size;
	char *content = malloc(file_size + 1);
	if (!content)
		return -1;

	if (file_size > 0 && read(fd, content, file_size) < 0)
		return -1;
	content[file_size] = '\0';
	/*
	turning the full text buffer into an array of lines, without copying each line, just storing pointers to parts of the original buffer*/
	char **lines = NULL;
	size_t count = 0;

	if (file_size > 0) {
		size_t LINE_SIZE_MAXs = file_size / 2 + 2;
		lines = malloc(sizeof(char *) * LINE_SIZE_MAXs);//assuming max no. of lines is f->sz/2 (+2 for safety)
		if (!lines)
			return -1;
		//strtok_r is the reentrant (thread-safe) version of strtok, divides the content based on \n as delimiters, ctx is used by strtok_r to know where it left off(its thread safe)
		char *ctx, *line = strtok_r(content, "\n", &ctx);
		while (line && count < LINE_SIZE_MAXs) {
			lines[count++] = line;
			line = strtok_r(NULL, "\n", &ctx);
		}
	}

	file_struct->fd = fd;
	file_struct->sz = file_size;
	file_struct->buf = content;
	file_struct->ln = lines;
	file_struct->n = count;

	return 0;
}




static void save(File *f) /* write-back  */
{
	lseek(f->fd, 0, SEEK_SET);
	ftruncate(f->fd, 0);
	for (int i = 0; i < f->n; ++i)
	{
		write(f->fd, f->ln[i], strlen(f->ln[i]));
		write(f->fd, "\n", 1);
	}
}

static void free_file(File *f) /* read-only close */
{
	fileUnlock(f->fd);//
	close(f->fd);
	free(f->buf);
	free(f->ln);
}



/* Split a line into ≤4 fields without altering the original string */
static int lineSplit(const char *src, char *fld[4])//to make the function static(fot this file only)
{
	/*
	__thread: This makes buf thread-local — each thread gets its own copy of buf.
	buf is where we copy src, since strtok_r modifies the string.
	*/
	static __thread char buf[LINE_SIZE_MAX];
	strncpy(buf, src, sizeof buf);
	char *sav, *tok;
	int k = 0;
	tok = strtok_r(buf, "|", &sav);
	while (tok && k < 4)
	{
		fld[k++] = tok;
		tok = strtok_r(NULL, "|", &sav);
	}
	return k;
}


/* Read the next non-empty line from socket and return it as an integer (-1 on error/EOF) */
static int getRecvChoice(int sock)
{
    char buf[32];

    while (1)
    {
        int len = recvString(sock, buf, sizeof(buf));
        if (len <= 0)
            return -1;  // Error or client closed connection

        // Trim leading and trailing whitespace
        buf[strcspn(buf, "\r\n")] = '\0';  // Remove newline
        char *p = buf;
        while (*p && (*p == ' ' || *p == '\t'))
            ++p;

        if (*p != '\0')  // If line is not blank
            return atoi(p);  // Convert to integer and return
    }
}

/* Determine if a line should be skipped (empty or comment) */
static int canSkipLine(const char *s)
{
	// Skip leading spaces/tabs
	while (*s == ' ' || *s == '\t')
		++s;
	// Skip if line is empty or starts with comment symbol
	return (*s == '\0' || *s == '#');
}

//search for row with given key
static int find_row(File *f, const char *key, int key_field)
{
	for (int i = 0; i < f->n; i++)
	{
		char *fld[4];
		if (canSkipLine(f->ln[i]))
			continue;

		int fields = lineSplit(f->ln[i], fld);
		if (fields > key_field && strcmp(fld[key_field], key) == 0)
			return i;
	}
	return -1;
}




static int adminCheck(int s)
{
	char u[100], p[100];
	sendString(s, "Enter username:\n");
	if (recvString(s, u, sizeof u) <= 0)
		return 0;
	sendString(s, "Enter password:\n");
	if (recvString(s, p, sizeof p) <= 0)
		return 0;
	u[strcspn(u, "\r\n")] = p[strcspn(p, "\r\n")] = '\0';
	return !strcmp(u, "Admin") && !strcmp(p, "11112222");
}


static int authorise(int sockfd, const char *filepath, char *username)
{
	char user[100], pass[100];
	sendString(sockfd, "Enter Username:\n");
	if (recvString(sockfd, user, sizeof user) <= 0)
		return 0;
	sendString(sockfd, "Enter Password:\n");
	if (recvString(sockfd, pass, sizeof pass) <= 0)
		return 0;

	user[strcspn(user, "\r\n")] = '\0';
	pass[strcspn(pass, "\r\n")] = '\0';

	File f;
	if (load_file(filepath, &f, F_RDLCK) < 0)
		return 0;

	int success = 0;
	for (int i = 0; i < f.n; ++i)
	{
		if (canSkipLine(f.ln[i]))
			continue;
		char *fields[4];
		int field_count = lineSplit(f.ln[i], fields);
		if (field_count >= 3 && 
			strcmp(fields[0], user) == 0 && 
			strcmp(fields[1], pass) == 0 && 
			fields[2][0] == '1')
		{
			strcpy(username, user);
			success = 1;
			break;
		}
	}
	free_file(&f);
	return success;
}



// ADMIN 


/*
Adds a new user (admin-controlled) to the specified file. If the username already
exists and the entry is valid, addition is rejected. Otherwise, the user is added or corrected.
*/
static void adminAdd(int sockfd, const char *filepath, const char *role_label)
{
	char username[100], password[100], prompt[100];

	// Ask for username
	snprintf(prompt, sizeof(prompt), "Enter %s username:\n", role_label);
	sendString(sockfd, prompt);
	if (recvString(sockfd, username, sizeof(username)) <= 0)
		return;

	// Ask for password
	sendString(sockfd, "Password:\n");
	if (recvString(sockfd, password, sizeof(password)) <= 0)
		return;

	// Clean up input
	username[strcspn(username, "\r\n")] = '\0';
	password[strcspn(password, "\r\n")] = '\0';

	File file;
	if (load_file(filepath, &file, F_WRLCK) < 0)
	{
		sendString(sockfd, "Error accessing user file\n");
		return;
	}

	int index = find_row(&file, username, 0);
	if (index != -1)
	{
		char *fields[4];
		int count = lineSplit(file.ln[index], fields);
		if (count >= 3)
		{
			sendString(sockfd, "User already exists\n");
			free_file(&file);
			return;
		}
	}

	// Add or overwrite entry
	if (index == -1)
	{
		//username didn't exist already, so can add
		dprintf(file.fd, "%s|%s|1|\n", username, password);//diretly write formatted output to a file descriptor
	}
	else
	{
		//index!=-1 but entry was corrupt (count<3), so will over-write
		char new_entry[LINE_SIZE_MAX];
		snprintf(new_entry, sizeof(new_entry), "%s|%s|1|", username, password);
		free(file.ln[index]);
		file.ln[index] = strdup(new_entry);
		save(&file);
	}

	free_file(&file);//unlocking here
	sendString(sockfd, "[OK] Added\n");
}
static void adminView(int sock, const char *filepath, const char *header)
{
	File file;
	if (load_file(filepath, &file, F_RDLCK) < 0)
	{
		sendString(sock, "Unable to access file.\n");
		return;
	}

	char title[64];
	snprintf(title, sizeof title, "\n%s List\n", header);
	sendString(sock, title);

	for (int i = 0; i < file.n; ++i)
	{
		if (canSkipLine(file.ln[i]))
			continue;

		char *fields[4];
		int count = lineSplit(file.ln[i], fields);

		if (count < 3)
			continue;

		char output[128];
		const char *status = (fields[2][0] == '1') ? "active" : "blocked";
		snprintf(output, sizeof output, " - %-12s  [%s]\n", fields[0], status);//%-12s  it left aligns a string of total length 12
		sendString(sock, output);
	}

	free_file(&file);
}



static void adminChangeStatus(int sockfd, int activate)
{
	char username[100];
	sendString(sockfd, "Enter Student username:\n");
	if (recvString(sockfd, username, sizeof username) <= 0)
		return;

		
	File file;
	if (load_file(STUDENT_FILE, &file, F_WRLCK) < 0)
	{
		sendString(sockfd, "Could not access student file\n");
		return;
	}
	username[strcspn(username, "\r\n")] = '\0';

	int index = find_row(&file, username, 0);
	if (index < 0)
	{
		sendString(sockfd, "User not found\n");
		free_file(&file);
		return;
	}

	char *fields[4];
	int field_count = lineSplit(file.ln[index], fields);
	if (field_count < 3)
	{
		sendString(sockfd, "Corrupt record\n");
		free_file(&file);
		return;
	}

	char updated_line[LINE_SIZE_MAX];
	snprintf(updated_line, sizeof updated_line, "%s|%s|%c|%s",
	         fields[0], fields[1], activate ? '1' : '0', (field_count == 4 ? fields[3] : ""));
	file.ln[index] = strdup(updated_line);

	save(&file);
	free_file(&file);
	sendString(sockfd, "Status updated successfully\n");
}


static void adminSetPassword(int sockfd, const char *filepath)
{
	char username[100], newpass[100];

	sendString(sockfd, "Enter Username:\n");
	if (recvString(sockfd, username, sizeof username) <= 0)
		return;

	sendString(sockfd, "New password:\n");
	if (recvString(sockfd, newpass, sizeof newpass) <= 0)
		return;

	username[strcspn(username, "\r\n")] = '\0';
	newpass[strcspn(newpass, "\r\n")] = '\0';

	File file;
	if (load_file(filepath, &file, F_WRLCK) < 0)
	{
		sendString(sockfd, "Failed to load file\n");
		return;
	}

	int index = find_row(&file, username, 0);
	if (index < 0)
	{
		sendString(sockfd, "User not found\n");
		free_file(&file);
		return;
	}

	char *fields[4];
	int field_count = lineSplit(file.ln[index], fields);

	char updated_line[LINE_SIZE_MAX];
	snprintf(updated_line, sizeof updated_line, "%s|%s|%s|%s",
	         fields[0], newpass, (field_count >= 3 ? fields[2] : "1"), (field_count == 4 ? fields[3] : ""));
	file.ln[index] = strdup(updated_line);

	sendString(sockfd, "Password changed successfully\n");
	save(&file);
	free_file(&file);
}



////////////////////////////////////////FACULTY 



/*
This function allows faculty to update the course name and seat limit
for a course identified by its code.
*/
static void facultyUpdateCourse(int sockfd, const char *faculty_id) {
    File f;
    if (load_file(COURSE_FILE, &f, F_WRLCK) < 0) {
        sendString(sockfd, "Error opening course file.\n");
        return;
    }
    int found = 0;
    sendString(sockfd, "Enter Course Code to update: \n");
    char course_code[FIELD_SIZE_MAX];
    if (recvString(sockfd, course_code, sizeof(course_code)) <= 0) {
		sendString(sockfd, "Failed to read Course Code.\n");
        free_file(&f);
        return;
    }
	course_code[strcspn(course_code, "\r\n")] = '\0';
	
	// printf("%dhola %s\n",strlen(course_code),course_code);fflush(stdout);
	course_code[strlen(course_code)]='\0';
    for (int i = 0; i < f.n; ++i) {
        if (canSkipLine(f.ln[i])) continue;

        char *fld[4];
        if (lineSplit(f.ln[i], fld) < 4) continue;

		// printf("%dhola %s\n",strlen(fld[0]),fld[0]);fflush(stdout);

        if (strcmp(fld[0], course_code) == 0) {
            found = 1;

            sendString(sockfd, "Enter new Course Name: \n");
            char new_name[FIELD_SIZE_MAX];
            if (recvString(sockfd, new_name, sizeof(new_name)) <= 0) break;

			new_name[strcspn(new_name, "\r\n")]='\0';
            sendString(sockfd, "Enter new Seat Limit: \n");
            char seat_buf[FIELD_SIZE_MAX];
            if (recvString(sockfd, seat_buf, sizeof(seat_buf)) <= 0) break;
            int new_limit = atoi(seat_buf);

            // Ensure new seat limit is not less than current enrollment
            int enrolled = atoi(fld[3]);
            if (new_limit < enrolled) {
                sendString(sockfd, "Error: Seat limit cannot be less than current enrollment.\n");
                break;
            }

            snprintf(f.ln[i], LINE_SIZE_MAX, "%s|%s|%d|%s", fld[0], new_name, new_limit, fld[3]);
            sendString(sockfd, "Course updated successfully.\n");
            break;
        }
    }

    if (!found) {
        sendString(sockfd, "Course not found.\n");
    }

    save(&f);
    free_file(&f);
}

static void facultyAddCourse(int s, const char *prof)
{
    char id[FIELD_SIZE_MAX], name[FIELD_SIZE_MAX], lim[16];
    sendString(s, "Course ID:\n");
    if (recvString(s, id, sizeof id) <= 0)
        return;
    sendString(s, "Course Name:\n");
    if (recvString(s, name, sizeof name) <= 0)
        return;
    sendString(s, "Seat Limit:\n");
    if (recvString(s, lim, sizeof lim) <= 0)
        return;
    id[strcspn(id, "\r\n")] = '\0';
    name[strcspn(name, "\r\n")] = '\0';
    int limit = atoi(lim);

    // Load courses with write lock
    File fc;
    if (load_file(COURSE_FILE, &fc, F_WRLCK) < 0) {
        sendString(s, "[ERROR] Could not load course file\n");
        return;
    }

    // Check if course ID already exists in course file
    int row = find_row(&fc, id, 0);
    if (row >= 0) {
        // Course already exists in course file
        sendString(s, "[ERROR] Course ID already exists\n");
        free_file(&fc);
        return;
    }

    // Add new course line
    fc.ln = realloc(fc.ln, (fc.n + 1) * sizeof(char *));
    if (!fc.ln) {
        sendString(s, "[ERROR] Memory allocation failed\n");
        free_file(&fc);
        return;
    }
    char *line = malloc(strlen(id) + strlen(name) + 32);
    sprintf(line, "%s|%s|%d|0", id, name, limit);
    fc.ln[fc.n++] = line;
    save(&fc);
    free_file(&fc);

    // Now add course to professor's list
    File ff;
    if (load_file(FACULTY_FILE, &ff, F_WRLCK) < 0) {
        sendString(s, "[ERROR] Could not load faculty file\n");
        return;
    }
    int prow = find_row(&ff, prof, 0);
    if (prow >= 0) {
        char *fld[4];
        int k = lineSplit(ff.ln[prow], fld);
        if (k >= 3 && fld[2][0] == '1') { // active faculty
            char list[LIST_SIZE_MAX] = "";
            if (k == 4 && strlen(fld[3]) > 0) {
                // Check if course already in faculty's course list
                char *tmp_list = strdup(fld[3]);
                char *token, *ctx;
                int found = 0;
                for (token = strtok_r(tmp_list, ",", &ctx); token != NULL; token = strtok_r(NULL, ",", &ctx)) {
                    if (strcmp(token, id) == 0) {
                        found = 1;
                        break;
                    }
                }
                free(tmp_list);

                if (found) {
                    sendString(s, "[ERROR] Course already assigned to you\n");
                    free_file(&ff);
                    return;
                }
                // Append new course
                snprintf(list, sizeof list, "%s,%s", fld[3], id);
            } else {
                strcpy(list, id);
            }

            char newline[LINE_SIZE_MAX];
            snprintf(newline, sizeof newline, "%s|%s|%s|%s",
                     fld[0], fld[1], fld[2], list);
            ff.ln[prow] = strdup(newline);
        } else {
            sendString(s, "[ERROR] Faculty inactive or malformed record\n");
            free_file(&ff);
            return;
        }
    } else {
        sendString(s, "[ERROR] Faculty not found\n");
        free_file(&ff);
        return;
    }

    save(&ff);
    free_file(&ff);
    sendString(s, "Course added successfully\n");
}


static void facultyRemoveCourse(int s, const char *who)
{
	char cid[FIELD_SIZE_MAX];
	sendString(s, "Course ID to remove:\n");
	if (recvString(s, cid, sizeof cid) <= 0)
		return;
	cid[strcspn(cid, "\r\n")] = '\0';

	//remove from catalogue 
	File fc;
	load_file(COURSE_FILE, &fc, F_WRLCK);
	int found = find_row(&fc, cid, 0);
	if (found < 0)
	{
		free_file(&fc);
		sendString(s, "Course not found\n");
		return;
	}

	char *course_fields[4];
	int parts = lineSplit(fc.ln[found], course_fields);
	if (parts != 4) {
		free_file(&fc);
		sendString(s, "Malformed course entry\n");
		return;
	}

	int enrolled = atoi(course_fields[3]);
	if (enrolled != 0) {
		free_file(&fc);
		sendString(s, "Some students are enrolled to the course, can't remove it\n");
		return;
	}
	// // do NOT free the pointer (may belong to fc.buf) - just shift   
	// memmove(&fc.ln[found], &fc.ln[found + 1], (fc.n - found - 1) * sizeof(char *));//just moving this entry in the buffer, so we can delete 
	// fc.n--;
	// save(&fc);


	// free(fc.ln[found]); // Free the line being removed
	// Shift all remaining lines up
	for (int i = found; i < fc.n - 1; ++i)
	    fc.ln[i] = fc.ln[i + 1];
	fc.n--;
	save(&fc);
	free_file(&fc);
	
	File ff;
	load_file(FACULTY_FILE, &ff, F_WRLCK);
	int prow = find_row(&ff, who, 0);
	if (prow >= 0)
	{
		char *fld[4];
		int k = lineSplit(ff.ln[prow], fld);
		if (k == 4 && fld[2][0] == '1')
		{
			char newlist[LIST_SIZE_MAX] = "";
			int first = 1;
			char *sub, *sv;
			sub = strtok_r(fld[3], ",", &sv);
			while (sub)
			{
				if (strcmp(sub, cid))
				{
					if (!first)
						strcat(newlist, ",");
					strcat(newlist, sub);
					first = 0;
				}
				sub = strtok_r(NULL, ",", &sv);
			}
			char newline[LINE_SIZE_MAX];
			snprintf(newline, sizeof newline, "%s|%s|%s|%s",
					 fld[0], fld[1], fld[2], newlist);
			ff.ln[prow] = strdup(newline);
		}
	}
	sendString(s, "Course removed successfully\n");
	save(&ff);
}


static void facultyViewOfferingCourses(int sockfd, const char *faculty_user)
{
	// Load faculty file to get the courses taught by the professor
	char course_list[LIST_SIZE_MAX] = "";

	File faculty;
	if (load_file(FACULTY_FILE, &faculty, F_RDLCK) >= 0) {
		int row = find_row(&faculty, faculty_user, 0);
		if (row >= 0) {
			char *fields[4];
			int n = lineSplit(faculty.ln[row], fields);
			if (n == 4 && fields[2][0] == '1')
				strcpy(course_list, fields[3]);
		}
		free_file(&faculty);
	}

	if (course_list[0] == '\0') {
		sendString(sockfd, "No courses exist\n");
		return;
	}

	// Split course list
	char *course_id, *course_ctx;
	char *allCourses[LIST_SIZE_MAX] = { NULL };
	int i = 0;

	course_id = strtok_r(course_list, ",", &course_ctx);
	while (course_id && i < LIST_SIZE_MAX) {
		allCourses[i] = strdup(course_id);
		i++;
		course_id = strtok_r(NULL, ",", &course_ctx);
	}

	// Load course file and print each course with details
	File fc;
	if (load_file(COURSE_FILE, &fc, F_RDLCK) < 0) {
		for (int j = 0; j < i; ++j) free(allCourses[j]);
		return;
	}

	for (int j = 0; j < i; ++j) {
		int found = find_row(&fc, allCourses[j], 0);
		if (found >= 0) {
			char *fld[4];
			int parts = lineSplit(fc.ln[found], fld);
			if (parts == 4) {
				// Output format: code, name, limit, enrolled
				dprintf(sockfd, "%s (code) | %s (name) | %s (limit) | %s (current enrollments)\n",
				        fld[0], fld[1], fld[2], fld[3]);
			}
		}
		free(allCourses[j]);
	}
	free_file(&fc);
}



static void facultyChangePwd(int s, const char *who)
{
	char pw[FIELD_SIZE_MAX];
	sendString(s, "New password:\n");
	if (recvString(s, pw, sizeof pw) <= 0)
		return;
	pw[strcspn(pw, "\r\n")] = '\0';

	File ff;
	load_file(FACULTY_FILE, &ff, F_WRLCK);
	int prow = find_row(&ff, who, 0);
	if (prow >= 0)
	{
		char *fld[4];
		int k = lineSplit(ff.ln[prow], fld);
		if (k >= 3 && fld[2][0] == '1')
		{
			char newline[LINE_SIZE_MAX];
			snprintf(newline, sizeof newline, "%s|%s|%s|%s",
					 fld[0], pw, fld[2], (k == 4 ? fld[3] : ""));
			ff.ln[prow] = strdup(newline);
			save(&ff);
			sendString(s, " Password changed successfully\n");
			return;
		}
	}
	free_file(&ff);
	sendString(s, "Account is blocked – cannot change password\n");
}

////////////////////////////////STUDENT 

static void studentEnroll(int s, const char *user)
{
	char cid[FIELD_SIZE_MAX];
	sendString(s, "Course ID to enroll:\n");
	if (recvString(s, cid, sizeof cid) <= 0)
		return;
	cid[strcspn(cid, "\r\n")] = '\0';

	File fc;
	load_file(COURSE_FILE, &fc, F_WRLCK);
	int row = find_row(&fc, cid, 0);
	if (row < 0)
	{
		free_file(&fc);
		sendString(s, "Course not found\n");
		return;
	}

	char *fld[4];
	int k = lineSplit(fc.ln[row], fld);
	int limit = atoi(fld[2]), filled = atoi(fld[3]);
	if (filled >= limit)
	{
		free_file(&fc);
		sendString(s, "Course full\n");
		return;
	}
	filled++;
	char newline[LINE_SIZE_MAX];
	snprintf(newline, sizeof newline, "%s|%s|%d|%d", fld[0], fld[1], limit, filled);
	fc.ln[row] = strdup(newline);
	save(&fc);

	/* add to student record */
	File fs;
	load_file(STUDENT_FILE, &fs, F_WRLCK);
	int srow = find_row(&fs, user, 0);
	if (srow >= 0)
	{
		char *sfld[4];
		k = lineSplit(fs.ln[srow], sfld);
		char list[LIST_SIZE_MAX] = "";
		if (k == 4 && strlen(sfld[3]))
			snprintf(list, sizeof list, "%s,%s", sfld[3], cid);
		else
			strcpy(list, cid);
		snprintf(newline, sizeof newline, "%s|%s|%s|%s",
				 sfld[0], sfld[1], sfld[2], list);
		fs.ln[srow] = strdup(newline);
	}
	save(&fs);
	sendString(s, "Enrolled successfully\n");
}

static void studentUnenroll(int s, const char *user)
{
	char cid[FIELD_SIZE_MAX];
	sendString(s, "Course ID to drop:\n");
	if (recvString(s, cid, sizeof cid) <= 0)
		return;
	cid[strcspn(cid, "\r\n")] = '\0';

	File fs;
	load_file(STUDENT_FILE, &fs, F_WRLCK);
	int srow = find_row(&fs, user, 0);
	if (srow < 0)
	{
		free_file(&fs);
		return;
	}

	char *sfld[4];
	int k = lineSplit(fs.ln[srow], sfld);
	char newlist[LIST_SIZE_MAX] = "";
	int first = 1, had = 0;
	if (k == 4)
	{
		char *sub, *sv;
		sub = strtok_r(sfld[3], ",", &sv);
		while (sub)
		{
			if (strcmp(sub, cid))
			{
				if (!first)
					strcat(newlist, ",");
				strcat(newlist, sub);
				first = 0;
			}
			else
				had = 1;
			sub = strtok_r(NULL, ",", &sv);
		}
	}
	if (!had)
	{
		free_file(&fs);
		sendString(s, "Not enrolled in that course\n");
		return;
	}

	char newline[LINE_SIZE_MAX];
	snprintf(newline, sizeof newline, "%s|%s|%s|%s",
			 sfld[0], sfld[1], sfld[2], newlist);
	fs.ln[srow] = strdup(newline);
	save(&fs);

	File fc;
	load_file(COURSE_FILE, &fc, F_WRLCK);
	int row = find_row(&fc, cid, 0);
	if (row >= 0)
	{
		char *fld[4];
		k = lineSplit(fc.ln[row], fld);
		int filled = atoi(fld[3]);
		if (filled > 0)
			filled--;
		snprintf(newline, sizeof newline, "%s|%s|%s|%d", fld[0], fld[1], fld[2], filled);
		fc.ln[row] = strdup(newline);
	}
	save(&fc);
	sendString(s, "[OK] Unenrolled\n");
}

static void studentView(int s, const char *user)
{
	File fs;
	load_file(STUDENT_FILE, &fs, F_RDLCK);
	int srow = find_row(&fs, user, 0);
	if (srow < 0)
	{
		free_file(&fs);
		return;
	}

	char list[LIST_SIZE_MAX] = "";
	char *fld[4];
	int k = lineSplit(fs.ln[srow], fld);
	if (k == 4)
		strncpy(list, fld[3], sizeof list);
	free_file(&fs);

	if (!strlen(list))
	{
		sendString(s, "No courses enrolled\n");
		return;
	}

	File fc;
	load_file(COURSE_FILE, &fc, F_RDLCK);
	sendString(s, "Enrolled:\n");
	char *cid, *sv;
	cid = strtok_r(list, ",", &sv);
	while (cid)
	{
		int row = find_row(&fc, cid, 0);
		if (row >= 0)
		{
			char *fld2[4];
			k = lineSplit(fc.ln[row], fld2);
			char line[LINE_SIZE_MAX];
			snprintf(line, sizeof line, " - %s : %s\n", fld2[0], fld2[1]);
			sendString(s, line);
		}
		cid = strtok_r(NULL, ",", &sv);
	}
	free_file(&fc);
}

static void studentChangePassword(int s, const char *user)
{
	char pw[FIELD_SIZE_MAX];
	sendString(s, "New password:\n");
	if (recvString(s, pw, sizeof pw) <= 0)
		return;
	pw[strcspn(pw, "\r\n")] = '\0';

	File fs;
	load_file(STUDENT_FILE, &fs, F_WRLCK);
	int row = find_row(&fs, user, 0);
	if (row >= 0)
	{
		char *fld[4];
		int k = lineSplit(fs.ln[row], fld);
		char newline[LINE_SIZE_MAX];
		snprintf(newline, sizeof newline, "%s|%s|%s|%s",
				 fld[0], pw, fld[2], (k == 4 ? fld[3] : ""));
		fs.ln[row] = strdup(newline);
	}
	save(&fs);
	sendString(s, "Password changed successfully\n");
}


static void facultyScreen(int s, const char *faculty_user)
{
	const char *menu =
		"\n........ Welcome to Faculty Menu ........\n"
		"1. View Offering Courses\n"
		"2. Add New Course \n"
		"3. Remove Course from Catalog\n"
		"4. Update Course Details\n"
		"5. Change Password  \n"
		"6. Logout and Exit\nChoice:\n";

	for (;;)
	{
		sendString(s, menu);
		int c = getRecvChoice(s);
		if (c == -1)
			return;
		if (c == 1)
			facultyViewOfferingCourses(s, faculty_user);
		else if (c == 2)
			facultyAddCourse(s, faculty_user);
		else if (c == 3)
			facultyRemoveCourse(s, faculty_user);
		else if (c == 4)
			facultyUpdateCourse(s,faculty_user);//here we can change the course name and the limit on enrollments
		else if (c == 5)
			facultyChangePwd(s, faculty_user);
		else if(c==6)
			return;
		else
			sendString(s, "Invalid choice\n");
	}
}

static void studentScreen(int s, const char *studentName)
{
	const char *menu =
		"\n........ Welcome to Student Menu ........\n"
		"1. Enroll in Course   \n"
		"2. Drop Course        \n"
		"3. View Enrolled Courses\n"
		"4. Change Password    (newPwd)\n"
		"5. Logout and Exit\nEnter Your Choice:\n";

	for (;;)
	{
		sendString(s, menu);
		int c = getRecvChoice(s);
		if (c == -1)
			return;
		if (c == 1)
			studentEnroll(s, studentName);
		else if (c == 2)
			studentUnenroll(s, studentName);
		else if (c == 3)
			studentView(s, studentName);
		else if (c == 4)
			studentChangePassword(s, studentName);
		else if (c == 5)
			return;
		else
			sendString(s, "Invalid choice\n");
	}
}

static void adminScreen(int s)
{
	const char *menu =
		"\n........ Welcome to Admin Menu ........\n"
		"1. Add Student     \n"
		"2. View Student List\n"
		"3. Add Faculty      \n"
		"4. View Faculty List\n"
		"5. Activate Student\n"
		"6. Block Student   \n"
		"7. Modify Student Details\n"
		"8. Modify Faculty Details\n"
		"9. Logout and Exit\nEnter Your Choice:\n";

	for (;;)
	{
		sendString(s, menu);
		int c = getRecvChoice(s);
		if (c == -1)
			return;
		if (c == 1)
			adminAdd(s, STUDENT_FILE, "Student");
		else if (c == 2)
			adminView(s, STUDENT_FILE, "Student");
		else if (c == 3)
			adminAdd(s, FACULTY_FILE, "Faculty");
		else if (c == 4)
			adminView(s, FACULTY_FILE, "Faculty");
		else if (c == 5)
			adminChangeStatus(s, 1);
		else if (c == 6)
			adminChangeStatus(s, 0);
		else if (c == 7)
			adminSetPassword(s, STUDENT_FILE);
		else if (c == 8)
			adminSetPassword(s, FACULTY_FILE);
		else if (c == 9)
			return;
		else
			sendString(s, "Invalid choice\n");
	}
}


// per-client thread 
static void *clientFunction(void *param)
{
	int s = *(int *)param;
	free(param);

	sendString(s, "................Welcome Back to Academia :: Course Registration................\n"
				 "Login Type\n"
				 "Enter Your Choice { 1.Admin , 2.Professor , 3.Student }: \n");

	int choice = getRecvChoice(s);
	if (choice == -1)
	{
		close(s);
		return NULL;
	}

	if (choice == 1)
	{
		if (!adminCheck(s))
		{
			sendString(s, "Authentication Failed: Invalid credentials\n");
			close(s);
			return NULL;
		}
		sendString(s, "Admin authentication successful\n");
		adminScreen(s);
	}
	else if (choice == 2)
	{
		char user[64] = "";
		if (!authorise(s, FACULTY_FILE, user))
		{
			sendString(s, "Authentication Failed: Invalid credentials\n");
			close(s);
			return NULL;
		}
		sendString(s, "Faculty authentication successful\n");
		facultyScreen(s, user);
	}
	else if (choice == 3)
	{
		char user[64] = "";
		if (!authorise(s, STUDENT_FILE, user))
		{
			sendString(s, "Authentication Failed: Invalid credentials\n");
			close(s);
			return NULL;
		}
		sendString(s, "Student authentication successful\n");
		studentScreen(s, user);
	}
	else
		sendString(s, "Invalid choice\n");

	sendString(s, "Logged out successfully\n");
	close(s);
	return NULL;
}


int main(void)
{
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sa = {.sin_family = AF_INET,
							 .sin_addr.s_addr = INADDR_ANY,
							 .sin_port = htons(PORT)};
	bind(server_fd, (void *)&sa, sizeof sa);
	listen(server_fd, 16);
	printf(" Server listening on port %d\n", PORT);

	for (;;)
	{
		int csock = accept(server_fd, NULL, NULL);
		int *p = malloc(sizeof(int));
		*p = csock;
		pthread_t t;
		pthread_create(&t, NULL, clientFunction, p);
		pthread_detach(t);
	}
}
