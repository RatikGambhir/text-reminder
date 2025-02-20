#include <stdio.h>
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>

// finding if the reminder message contains a date, if so, we add this date to the reminder
int regex_contains_date(char *message, regmatch_t *match, regex_t regex) {
     int regex_result;

            const char *pattern = "([0-9]{1,2}/[0-9]{1,2}(/[0-9]{4})?)";

            
            regex_result = regcomp(&regex, pattern, REG_EXTENDED);
            if (regex_result) {
                printf("Could not compile regex\n");
                return 0;
            }

           
            return regexec(&regex, message, 1, match, 0);


}

// time regex. Finding if the text contains 8:30PM or 830PM format. PM or AM must be directly after the time
int regex_contains_time(char *message, regmatch_t *match, regex_t regex) {
     int regex_result;

            const char *pattern = "([0-9]:?[0-9][0-9][AaPp][Mm])";

            regex_result = regcomp(&regex, pattern, REG_EXTENDED);
            if (regex_result) {
                printf("Could not compile regex\n");
                return 0;
            }

        
            return regexec(&regex, message, 1, match, 0);


}

//After we process a reminder message, we want to mark it as read so that we don't process a reminder message more than once
void mark_message_as_read(sqlite3 *db, int message_id) {
    sqlite3_stmt *stmt;
    int rc;
    const char *sql = "UPDATE message SET is_read = 1 WHERE ROWID = ?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare is_read update: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    }

    sqlite3_bind_int(stmt, 1, message_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to update message: %s\n", sqlite3_errmsg(db));
    } else {
        printf("✅ Marked message %d as read.\n", message_id);
    }

    sqlite3_finalize(stmt);
}

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;
    time_t t;
    regex_t regex;
    regmatch_t match[1]; // Stores the position of the match
    regmatch_t time_match[1]; // Stores the position of the match


    time(&t); // Get current time


    rc = sqlite3_open("/Users/ratikgambhir/Library/Messages/chat.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    long long apple_current_time = ((long)t - 978307200) * 1000000000;
    long long reminder_window = apple_current_time - 10800000000000;

    const char *sql =
        "SELECT message.text, message.ROWID, handle.id AS sender "
        "FROM message "
        "JOIN handle ON message.handle_id = handle.ROWID "
        "WHERE message.text IS NOT NULL "
        "AND message.is_read = 0 "
        "AND message.is_from_me = 0 "
        "AND message.date >= ? "
        "ORDER BY message.date DESC;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare text message query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sqlite3_bind_int64(stmt, 1, reminder_window);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        int message_id = sqlite3_column_int(stmt, 1);
        const unsigned char *number = sqlite3_column_text(stmt, 2);

        if (text) {
            size_t text_len = strlen((const char *)text);
            char *message = (char *)malloc(text_len + 1);

            if (message == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                continue; // Skip this row if allocation fails
            }

            printf("Message from number: %s\n", (const char *)number);
            strcpy(message, (char *)text);

            char command[256];

            //Ideally here we would like to query the sqlite db that has contact info, but ICloud was giving me trouble
            snprintf(command, sizeof(command),
                     "osascript -e 'tell application \"Contacts\" to get name of first person whose value of phones contains \"%s\"'",
                     (const char *)number);

            FILE *fp = popen(command, "r");
            if (fp == NULL) {
                printf("Failed to run AppleScript\n");
                return 1;
            }

            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("Contact name found: %s", buffer);
            } else {
                printf("No contact name found\n");
            }

            const char *found = strcasestr(message, "Reminder:");

            int date_found = regex_contains_date(message, match, regex);
            int time = regex_contains_time(message, time_match, regex);

            if (found != NULL) {
                char command[512];
                found += strlen("Reminder:") + 1;
                printf("Message reminder: %s\n", found);
            
                struct tm *local_time;
                char formatted_date[100]; 

                local_time = localtime(&t);

                strftime(formatted_date, sizeof(formatted_date), "%B %d, %Y %I:%M %p", local_time);

                printf("Formatted Date: %s\n", formatted_date);

        //Do switch statement here for all the diff cases
                if (date_found == 1) {
                    printf("No date FOUND");
                    if(time == 1) {

                        printf("No date and time found");
                        
                        snprintf(command, sizeof(command),
                             "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                             "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                             found, formatted_date, buffer);
                    } else {
                            printf("No date but time found");
                    // TODO add time column is a time exists in message
                    char extracted_time[50] = ""; 
                    int start = time_match[0].rm_so;
                    int end = time_match[0].rm_eo;
                    int match_len = end - start;
                    strncpy(extracted_time, message + start, match_len);
                    extracted_time[match_len] = '\0';

                                            printf("No date but time found");


                    snprintf(command, sizeof(command),
                             "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                             "with properties {name:\"%s\", due date:date (short date string of (current date) & \" %s\"), body:\"%s\"}'",
                             found, extracted_time, buffer);
                    }
                        
                } else {
                                       char extracted_date[50] = ""; 
                    int start = match[0].rm_so;
                    int end = match[0].rm_eo;
                    int match_len = end - start;

                    strncpy(extracted_date, message + start, match_len);

                    int year = local_time->tm_year + 1900;  // Get current year
        
    snprintf(formatted_date, sizeof(formatted_date), "%s/%d", extracted_date, year);


                    extracted_date[match_len] = '\0'; 

                    printf("Formatted DATE: %s", formatted_date);

                    snprintf(command, sizeof(command),
                             "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                             "with properties {name:\"%s\", due date:date \"%s\" ,body:\"%s\"}'",
                             found, formatted_date, buffer);
                }

                system(command);
                mark_message_as_read(db, message_id);
            }

            regfree(&regex);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}