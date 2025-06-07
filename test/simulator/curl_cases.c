#include "../../src/properties.h"
#include "../../src/static.h"
#include "../../src/utils/assert2.h"
#include "../../src/utils/logger.h"
#include "connection_cases.h"
#include <curl/curl.h>
#include <dirent.h>
#include <sqlite3.h>
#include <string.h>
#include <sys/stat.h>

#define INDEX_PROJECTS_SQL "SELECT id, title FROM Project;"
#define INDEX_POSTS_SQL "SELECT id, title FROM Post;"

struct write_data {
  char *response;
  size_t total_written;
};

unsigned int write_callback(void *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  strncat(userdata, ptr, size * nmemb);
  return size * nmemb;
}

unsigned int write_callback_with_len(void *ptr, size_t size, size_t nmemb,
                                     void *userdata) {
  struct write_data *data = (struct write_data *)userdata;
  size_t to_copy = size * nmemb;
  memcpy(data->response + data->total_written, ptr, to_copy);
  data->total_written += to_copy;
  return to_copy;
}

void *start_curl_get_request(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  CURL *curl;
  CURLcode res;
  char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
  char address[HTTP_HEADER_SIZE];
  snprintf(address, HTTP_HEADER_SIZE, "http://%s:%s", data->ip, data->port);

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, address);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");

    res = curl_easy_perform(curl);
    assert(res == CURLE_OK);

    char *content_type;
    long response_long;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_long);
    assert(response_long == 200);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    assert(content_type != NULL);
    assert(strstr(content_type, "text/html") != NULL);
    assert(response[0] != '\0');
    assert(strstr(response, "<body>") != NULL);
    assert(strstr(response, "</body>") != NULL);
    assert(strstr(response, "</html>") != NULL);

    curl_easy_cleanup(curl);
  }

  return 0;
}

void *start_curl_get_all_posts(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  sqlite3 *db;
  sqlite3_stmt *stmt;
  CURL *curl;
  CURLcode res;
  char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
  char address[HTTP_HEADER_SIZE];
  int rc;
  curl = curl_easy_init();
  assert(curl != NULL);
  rc = sqlite3_open(get_db_url(), &db);
  assert(rc == SQLITE_OK);
  rc = sqlite3_prepare_v2(db, INDEX_POSTS_SQL, -1, &stmt, NULL);
  assert(rc == SQLITE_OK);

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *project_id = (const char *)sqlite3_column_text(stmt, 0);
    const char *project_name = (const char *)sqlite3_column_text(stmt, 1);
    snprintf(address, HTTP_HEADER_SIZE, "https://%s:%s/posts/%s", data->ip,
             get_https_port(), project_id);

    curl_easy_setopt(curl, CURLOPT_URL, address);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);
    assert(res == CURLE_OK);

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    assert(response_code == 200);

    // Check if the response contains the expected HTML structure
    assert(strstr(response, "<body>") != NULL);
    assert(strstr(response, "</body>") != NULL);
    assert(strstr(response, "</html>") != NULL);
    assert(strstr(response, project_name) != NULL);

    memset(response, 0,
           REQUEST_RESPONSE_MAX_SIZE);    // Clear response for next iteration
    memset(address, 0, HTTP_HEADER_SIZE); // Clear address for next iteration
  }
  curl_easy_cleanup(curl);
  assert(rc == SQLITE_DONE);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return NULL;
}

void *start_curl_get_all_projects(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  sqlite3 *db;
  sqlite3_stmt *stmt;
  CURL *curl;
  CURLcode res;
  char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
  char address[HTTP_HEADER_SIZE];
  int rc;
  curl = curl_easy_init();
  assert(curl != NULL);
  rc = sqlite3_open(get_db_url(), &db);
  assert(rc == SQLITE_OK);
  rc = sqlite3_prepare_v2(db, INDEX_PROJECTS_SQL, -1, &stmt, NULL);
  assert(rc == SQLITE_OK);

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *project_id = (const char *)sqlite3_column_text(stmt, 0);
    const char *project_name = (const char *)sqlite3_column_text(stmt, 1);
    snprintf(address, HTTP_HEADER_SIZE, "https://%s:%s/projects/%s", data->ip,
             get_https_port(), project_id);

    curl_easy_setopt(curl, CURLOPT_URL, address);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);
    assert(res == CURLE_OK);

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    assert(response_code == 200);

    // Check if the response contains the expected HTML structure
    assert(strstr(response, "<body>") != NULL);
    assert(strstr(response, "</body>") != NULL);
    assert(strstr(response, "</html>") != NULL);
    assert(strstr(response, project_name) != NULL);

    memset(response, 0,
           REQUEST_RESPONSE_MAX_SIZE);    // Clear response for next iteration
    memset(address, 0, HTTP_HEADER_SIZE); // Clear address for next iteration
  }
  curl_easy_cleanup(curl);
  assert(rc == SQLITE_DONE);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return NULL;
}

void *start_curl_get_all_images(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  CURL *curl;
  CURLcode res;
  char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
  char address[HTTP_HEADER_SIZE + 100], file_path[HTTP_HEADER_SIZE + 100];
  curl = curl_easy_init();

  DIR *dir = opendir("static");
  assert(dir != NULL);
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    struct stat file_stat;
    if (strstr(entry->d_name, ".png") || strstr(entry->d_name, ".gif")) {
      snprintf(address, HTTP_HEADER_SIZE + 100, "https://%s:%s/%s", data->ip,
               get_https_port(), entry->d_name);
      snprintf(file_path, HTTP_HEADER_SIZE + 100, "static/%s", entry->d_name);

      stat(file_path, &file_stat);
      struct write_data wdata = {response, 0};
      curl_easy_setopt(curl, CURLOPT_URL, address);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_with_len);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wdata);
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      res = curl_easy_perform(curl);
      assert(res == CURLE_OK);

      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      assert(response_code == 200);

      assert(wdata.total_written == file_stat.st_size);

      memset(response, 0,
             REQUEST_RESPONSE_MAX_SIZE); // Clear response for next iteration
    }
  }
  curl_easy_cleanup(curl);
  closedir(dir);

  return NULL;
}

void *start_curl_get_ssl_request(void *arg) {
  struct connection_data *data = (struct connection_data *)arg;
  CURL *curl;
  CURLcode res;
  char response[REQUEST_RESPONSE_MAX_SIZE] = {0};
  char address[HTTP_HEADER_SIZE];
  snprintf(address, HTTP_HEADER_SIZE, "https://%s:%s", data->ip,
           get_https_port());

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, address);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);
    assert(res == CURLE_OK);

    char *content_type;
    long response_long;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_long);
    assert(response_long == 200);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    assert(content_type != NULL);
    assert(strstr(content_type, "text/html") != NULL);
    assert(response[0] != '\0');
    assert(strstr(response, "<body>") != NULL);
    assert(strstr(response, "</body>") != NULL);
    assert(strstr(response, "</html>") != NULL);

    curl_easy_cleanup(curl);
  }

  return 0;
}
