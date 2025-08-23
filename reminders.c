#include <stdio.h>
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>

#define HAS_DATE 1
#define HAS_TIME 2
#define HAS_NOTES 4

const char *DATE_REGEX = "[0-9]{1,2}/[0-9]{1,2}(/[0-9]{4})?";
const char *TIME_REGEX = "([0-9]{1,4}(:[0-9]{2})?[ ]*[AaPp][Mm])";

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


/**
 * Evaluates a regex pattern on a given source text.
 */
int evaluate_regex(const char *source_text, regmatch_t *match, regex_t regex, const char *regex_pattern) {
    int regex_result = regcomp(&regex, regex_pattern, REG_EXTENDED);
    if (regex_result) {
        printf("Could not compile regex: %s\n", regex_pattern);
        return 0;
    }
    return regexec(&regex, source_text, 1, match, 0);
}


// time regex. Finding if the text contains any of the following formats: 830 PM, 830PM, 8:30PM, 8:30 PM, 8 PM, 8PM (lowercase AM and PM is also allowed)
int regex_contains_time(char *message, regmatch_t *match, regex_t *regex) {
     int regex_result;


         const char *pattern = "([0-9]{1,2}(:[0-9]{2})?[ ]*[AaPp][Mm])";

            regex_result = regcomp(regex, pattern, REG_EXTENDED | REG_ICASE);
            if (regex_result) {
                printf("Could not compile regex\n");
                return 0;
            }

        
            return regexec(regex, message, 1, match, 0);


}


/**
 * Marks a message as read in the SQLite database.
 */
void mark_message_as_read(sqlite3 *db, int message_id) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE message SET is_read = 1 WHERE ROWID = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare is_read update: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_int(stmt, 1, message_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to update message: %s\n", sqlite3_errmsg(db));
    } else {
        printf("✅ Marked message %d as read.\n", message_id);
    }

    sqlite3_finalize(stmt);
}



void extract_regex(regmatch_t *match, const char *message, char *extracted_regex) {
    int start = match[0].rm_so;
    int end = match[0].rm_eo;
    int match_len = end - start;
    strncpy(extracted_regex, message + start, match_len);
    extracted_regex[match_len] = '\0';  // Properly null terminate the string
}

void get_contact_name(const unsigned char *number, char *contact_buffer, char *command) {
        

            //Ideally here we would like to query the sqlite db that has contact info, but ICloud was giving me trouble
            snprintf(command, sizeof(command),
                     "osascript -e 'tell application \"Contacts\" to get name of first person whose value of phones contains \"%s\"'",
                     (const char *)number);

            FILE *fp = popen(command, "r");
            if (fp == NULL) {
                printf("Failed to run AppleScript\n");
               
            }

            if (fgets(contact_buffer, sizeof(contact_buffer), fp) == NULL) {
                strcpy(contact_buffer, number); // Copy the string
            } 
}

int prepare_sqlite_query(char *sql_query, sqlite3 *db, sqlite3_stmt *stmt) {
    int rc;

    if(db == NULL) {
        rc = sqlite3_open("/Users/ratikgambhir/Library/Messages/chat.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    }

    rc = sqlite3_prepare_v2(db, sql_query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare message query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    return rc;


}

void format_date_time(char *date, char *time, char *formatted_date_time, size_t output_size) {
    struct tm datetime = {0};
    char temp_datetime[100];
    snprintf(temp_datetime, sizeof(temp_datetime), "%s %s", date, time);

    if(date == NULL && time == NULL) {
        // add current date time

    } else if(date == NULL) {
        // add current date with given time
    } else if(time == NULL) {
        // add given date with current time
    } else {
        // combine given date and time
    }
    printf("temp datetime: %s\n", temp_datetime);
    

    strftime(formatted_date_time, output_size, "%m/%d/%Y %I:%M %p", &datetime);
}

void gen_reminder_command(const char *message, char *contact, const char *reminder_content, char *command, size_t command_size) {
    struct tm *local_time;
    char formatted_current_date[50];
    char formatted_current_time[50];
    regex_t date_regex, time_regex;
    regmatch_t date_match_pointer[1];
    regmatch_t time_match_pointer[1];
    time_t raw_time;
    time_t t;

    printf("Message: %s\n", message);

    int has_date = evaluate_regex(message, date_match_pointer, date_regex, DATE_REGEX);
    int has_time = evaluate_regex(message, time_match_pointer, time_regex, TIME_REGEX);
    const char *reminder_notes = strcasestr(message, "Notes:");

    int condition = (has_date != 1 ? HAS_DATE : 0) | 
                    (has_time != 1 ? HAS_TIME : 0) | 
                    (reminder_notes != NULL ? HAS_NOTES : 0);

    printf("Condition: %d\n", condition);
    printf("NOTES?: %s", reminder_notes);

    time(&raw_time);
    local_time = localtime(&raw_time);
    strftime(formatted_current_date, sizeof(formatted_current_date), "%m/%d/%Y", local_time);
    strftime(formatted_current_time, sizeof(formatted_current_time), "%I:%M:%S %p", local_time);
    printf("current date: %s\n", formatted_current_date);
    printf("current time: %s\n", formatted_current_time);

    switch (condition) {
        case 0:
            {
                char formatted_current_datetime[100];
            snprintf(formatted_current_datetime, sizeof(formatted_current_datetime), "%s %s", formatted_current_date, formatted_current_time);
            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_current_datetime, contact);
            break;
            }
        
        case HAS_DATE: {
            printf("HAS DATE\n");
            char formatted_extracted_date[50] = ""; 
            char extracted_date[50] = "";
            
            extract_regex(date_match_pointer, message, extracted_date);
      

            if (strlen(extracted_date) <= 5) {
                int year = local_time->tm_year + 1900;  // Get current year
                snprintf(formatted_extracted_date, sizeof(formatted_extracted_date), "%s/%d", extracted_date, year);
            } else {
                snprintf(formatted_extracted_date, sizeof(formatted_extracted_date), "%s", extracted_date);
            }

            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_extracted_date, contact);
            break;
        }

        case HAS_TIME: {
            printf("HAS TIME\n");
            char extracted_time[50] = ""; 
            

            extract_regex(time_match_pointer, message, extracted_time);

            printf("extracted time: %s\n", extracted_time);
            snprintf(formatted_current_date, sizeof(formatted_current_date), "%s %s", formatted_current_date, extracted_time);


            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_current_date, contact);
            
            break;
        }

        case HAS_DATE | HAS_TIME: {
                        printf("HAS TIME AND DATE");

            char extracted_time[50] = ""; 
            char extracted_date[50] = "";
            char formatted_date[40] = "";

            extract_regex(date_match_pointer, message, extracted_date);
            extract_regex(time_match_pointer, message, extracted_time);

            if (strlen(extracted_date) <= 5) {
                int year = local_time->tm_year + 1900;  // Get current year
                snprintf(formatted_date, sizeof(formatted_date), "%s/%d %s", extracted_date, year, extracted_time);
            } else {
                snprintf(formatted_date, sizeof(formatted_date), "%s %s", extracted_date, extracted_time);
            }

            printf("Formatted Date: %s\n", formatted_date);

            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_date, contact);
            break;
        }

        case HAS_NOTES: {
            printf("HAS NOTES\n");

            char notes[200];

            snprintf(notes, sizeof(notes), "From %s: %s", contact, reminder_notes += strlen("Notes:") + 1);
            
            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_current_date, notes);

            break;
        }

        case HAS_NOTES | HAS_DATE: {
            printf("HAS NOTES AND DATE\n");
            char notes[200];
            char formatted_date[30];
            char extracted_date[20] = "";

            extract_regex(date_match_pointer, message, extracted_date);

            if (strlen(extracted_date) <= 5) {
                int year = local_time->tm_year + 1900;  // Get current year
                snprintf(formatted_date, sizeof(formatted_date), "%s/%d", extracted_date, year);
            } else {
                snprintf(formatted_date, sizeof(formatted_date), "%s", extracted_date);
            }

            snprintf(notes, sizeof(notes), "From %s: %s", contact, reminder_notes += strlen("Notes:") + 1);

            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_date, notes);
            break;
        }

        case HAS_NOTES | HAS_TIME: {
            printf("HAS NOTES AND TIME\n");
            char notes[200];
            char formatted_date[30];
            char extracted_time[20] = ""; 

            extract_regex(time_match_pointer, message, extracted_time);
            snprintf(notes, sizeof(notes), "From %s: %s", contact, reminder_notes += strlen("Notes:") + 1);

            strftime(formatted_date, sizeof(formatted_date), "%-m/%d/%Y", local_time);
            snprintf(formatted_date, sizeof(formatted_date), "%s", extracted_time);

            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_date, notes);
            break;
        }

        case HAS_DATE | HAS_NOTES | HAS_TIME: {
            printf("HAS NOTES AND TIME AND DATE\n");
            char notes[200];
            char formatted_date[40];
            char extracted_time[20] = ""; 
            char extracted_date[20] = ""; 

            extract_regex(date_match_pointer, message, extracted_date);
            extract_regex(time_match_pointer, message, extracted_time);

            if (strlen(extracted_date) <= 5) {
                int year = local_time->tm_year + 1900;  
                snprintf(formatted_date, sizeof(formatted_date), "%s/%d %s", extracted_date, year, extracted_time);
            } else {
                snprintf(formatted_date, sizeof(formatted_date), "%s %s", extracted_date, extracted_time);
            }

            snprintf(notes, sizeof(notes), "From %s: %s", contact, reminder_notes += strlen("Notes:") + 1);

            snprintf(command, command_size,
                     "osascript -e 'tell application \"Reminders\" to make new reminder at list \"Reminders\" "
                     "with properties {name:\"%s\", due date:date \"%s\", body:\"%s\"}'",
                     reminder_content, formatted_date, notes);
            break;
        }
    }

    regfree(&date_regex);
    regfree(&time_regex);
}

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;
    time_t t; 
    regex_t date_regex_1, time_regex_1;
    regmatch_t date_match_pointer_1[1];
    regmatch_t time_match_pointer_1[1];

    char reminder_command[512];

    char *mock_message = "Reminder: Get Apples at whole foods from the silly goose amanda 8/4/2025 7:30 PM. Notes: Make sure they are silly goose ones";
    const char *reminder_message = strcasestr(mock_message, "Reminder:");

    reminder_message += strlen("Reminder:") + 1;

    gen_reminder_command(mock_message, "Ratik Gambhir", reminder_message, reminder_command, sizeof(reminder_command));

    printf("Reminder Command: %s", reminder_command);   

    system(reminder_command);
    

//          //  get_contact_name(number, contact_buffer, contact_command);

//             const char *reminder_message_mock = strcasestr(mock_message, "Reminder:");
//             reminder_message_mock += strlen("Reminder:") + 1;

//     char *contact_mock = "Ratik Gambhir";

//     gen_reminder_command(mock_message, contact_mock, reminder_message_mock, reminder_command);
// printf("Reminder Command: %s", reminder_command);
//         system(reminder_command);

        


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

        // if (text) {
        //     size_t text_len = strlen((const char *)text);
        //     char *message = (char *)malloc(text_len + 1);
        //     char contact_buffer[64];
        //     char contact_command[256];

        //     if (message == NULL) {
        //         fprintf(stderr, "Memory allocation failed\n");
        //         continue; // Skip this row if allocation fails
        //     }
       
        //     strcpy(message, (char *)text);

        //     char *mock_message = "Reminder: Get Apples at whole foods 9:30 PM";

        //  //  get_contact_name(number, contact_buffer, contact_command);

        //     const char *reminder_message = strcasestr(mock_message, "Reminder:");

        //     if (reminder_message != NULL) {
        //         char reminder_command[512];
        //         char *contact_mock = "Ratik Gambhir";
        //         reminder_message += strlen("Reminder:") + 1;
              
        //         gen_reminder_command(mock_message, contact_buffer, reminder_message, reminder_command, sizeof(reminder_command));

        //         printf("Reminder Command: %s", reminder_command);

        //         system(reminder_command);
        //         mark_message_as_read(db, message_id);
        //     }

        // }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
