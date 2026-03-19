/* ollama.c, Ollama interaction view, Val Emacs */

#include "header.h"
#include <curl/curl.h>
#include <json-c/json.h>

#define MAX_MESSAGES    100
#define MAX_CONTENT     32768
#define INPUT_BUF       4096
#define MAX_CODE_BLOCKS 50
#define CODE_BLOCK_BUF  65536
#define OLLAMA_URL      "http://localhost:11434/api/chat"
#define DEFAULT_MODEL   "qwen3.5:397b-cloud"

typedef struct {
    int is_user;
    char content[MAX_CONTENT];
} message_t;

typedef struct {
    char code[CODE_BLOCK_BUF];
    int len;
    char language[64];
} code_block_t;

static message_t messages[MAX_MESSAGES];
static int nmessages;
static code_block_t code_blocks[MAX_CODE_BLOCKS];
static int ncode_blocks;
static char input_buf[INPUT_BUF];
static int input_pos;
static char model[64];
static int scroll_offset;

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;
    char *resp = (char *)userdata;
    strncat(resp, ptr, MAX_CONTENT - strlen(resp) - 1);
    return realsize;
}

static int ollama_chat(const char *prompt)
{
    CURL *curl;
    CURLcode res;
    long http_code;
    char response[MAX_CONTENT];
    struct json_object *req_obj, *msg_arr, *msg_obj;
    struct json_object *resp_obj, *resp_msg, *resp_content;
    struct curl_slist *headers = NULL;
    const char *json_str;

    curl = curl_easy_init();
    if (!curl) return -1;

    memset(response, 0, sizeof(response));

    headers = curl_slist_append(headers, "Content-Type: application/json");

    req_obj = json_object_new_object();
    json_object_object_add(req_obj, "model", json_object_new_string(model));
    json_object_object_add(req_obj, "stream", json_object_new_boolean(0));

    msg_arr = json_object_new_array();
    for (int i = 0; i < nmessages; i++) {
        msg_obj = json_object_new_object();
        json_object_object_add(msg_obj, "role", json_object_new_string(messages[i].is_user ? "user" : "assistant"));
        json_object_object_add(msg_obj, "content", json_object_new_string(messages[i].content));
        json_object_array_add(msg_arr, msg_obj);
    }
    msg_obj = json_object_new_object();
    json_object_object_add(msg_obj, "role", json_object_new_string("user"));
    json_object_object_add(msg_obj, "content", json_object_new_string(prompt));
    json_object_array_add(msg_arr, msg_obj);
    json_object_object_add(req_obj, "messages", msg_arr);

    json_str = json_object_to_json_string(req_obj);

    curl_easy_setopt(curl, CURLOPT_URL, OLLAMA_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    json_object_put(req_obj);

    if (res != CURLE_OK || http_code != 200) {
        return -1;
    }

    resp_obj = json_tokener_parse(response);
    if (!resp_obj) return -1;

    if (!json_object_object_get_ex(resp_obj, "message", &resp_msg) ||
        !json_object_object_get_ex(resp_msg, "content", &resp_content)) {
        json_object_put(resp_obj);
        return -1;
    }

    const char *content = json_object_get_string(resp_content);
    if (content && nmessages < MAX_MESSAGES) {
        messages[nmessages].is_user = 0;
        strncpy(messages[nmessages].content, content, MAX_CONTENT - 1);
        messages[nmessages].content[MAX_CONTENT - 1] = '\0';
        nmessages++;
    }

    json_object_put(resp_obj);
    return 0;
}

static void parse_code_blocks(const char *text)
{
    const char *p = text;
    ncode_blocks = 0;

    while (*p && ncode_blocks < MAX_CODE_BLOCKS) {
        if (p[0] == '`' && p[1] == '`' && p[2] == '`') {
            const char *end = p + 3;
            while (*end && *end != '`' && *end != '\n') end++;

            if (*end == '\n') {
                int lang_len = end - (p + 3);
                if (lang_len > 0 && lang_len < 64) {
                    strncpy(code_blocks[ncode_blocks].language, p + 3, lang_len);
                    code_blocks[ncode_blocks].language[lang_len] = '\0';
                } else {
                    code_blocks[ncode_blocks].language[0] = '\0';
                }

                const char *code_start = end + 1;
                const char *code_end = code_start;
                int found_end = 0;
                while (*code_end) {
                    if (code_end[0] == '`' && code_end[1] == '`' && code_end[2] == '`') {
                        found_end = 1;
                        break;
                    }
                    code_end++;
                }

                if (found_end) {
                    int code_len = code_end - code_start;
                    if (code_len > 0 && code_len < CODE_BLOCK_BUF) {
                        strncpy(code_blocks[ncode_blocks].code, code_start, code_len);
                        code_blocks[ncode_blocks].code[code_len] = '\0';
                        code_blocks[ncode_blocks].len = code_len;
                        ncode_blocks++;
                    }
                    p = code_end + 3;
                    while (*p == '\n') p++;
                } else {
                    p++;
                }
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
}

static int insert_code_block_at_line(int block_idx, int line_num)
{
    if (block_idx < 0 || block_idx >= ncode_blocks) return FALSE;

    char *code = code_blocks[block_idx].code;
    int code_len = code_blocks[block_idx].len;

    int curline, lastline;
    get_line_stats(&curline, &lastline);

    if (line_num > lastline) line_num = lastline;
    if (line_num < 1) line_num = 1;

    point_t target_point = line_to_point(line_num);
    if (target_point < 0) target_point = pos(curbp, curbp->b_ebuf);

    if (code_len >= curbp->b_egap - curbp->b_gap || growgap(curbp, code_len)) {
        curbp->b_point = movegap(curbp, target_point);
        memcpy(curbp->b_gap, code, code_len * sizeof(char_t));
        curbp->b_gap += code_len;
        curbp->b_point = pos(curbp, curbp->b_egap);
        curbp->b_flags |= B_MODIFIED;
        return TRUE;
    }
    return FALSE;
}

static void add_user_message(const char *text)
{
    if (nmessages >= MAX_MESSAGES) {
        memmove(messages, messages + 1, (MAX_MESSAGES - 1) * sizeof(message_t));
        nmessages--;
    }
    messages[nmessages].is_user = 1;
    strncpy(messages[nmessages].content, text, MAX_CONTENT - 1);
    messages[nmessages].content[MAX_CONTENT - 1] = '\0';
    nmessages++;
}

static void draw_ollama_view(void)
{
    int row = 1;
    int i;
    int block_counter = 0;

    erase();

    attron(A_REVERSE);
    mvaddstr(0, 0, " Ollama Chat [");
    addstr(model);
    if (scroll_offset > 0) {
        addstr("] ^");
    } else {
        addstr("]");
    }
    addstr(" | Esc/C-g=close | Enter=send | ,c=paste code | PgUp/PgDn=scroll");
    clrtoeol();
    attroff(A_REVERSE);

    int display_rows = LINES - 3;
    int start_idx = scroll_offset;
    if (start_idx >= nmessages) start_idx = nmessages - 1;
    if (start_idx < 0) start_idx = 0;

    for (i = start_idx; i < nmessages && row < display_rows; i++) {
        char *label = messages[i].is_user ? ">>> " : "<<< ";
        mvaddstr(row++, 0, label);

        const char *p = messages[i].content;

        while (*p && row < display_rows) {
            if (p[0] == '`' && p[1] == '`' && p[2] == '`') {
                const char *code_start = NULL;
                const char *code_end = NULL;
                char lang[64] = {0};

                const char *scan = p + 3;
                while (*scan) {
                    if (scan[0] == '`' && scan[1] == '`' && scan[2] == '`') {
                        break;
                    }
                    if (*scan == '\n') {
                        int lang_len = scan - (p + 3);
                        if (lang_len > 0 && lang_len < 64) {
                            strncpy(lang, p + 3, lang_len);
                            lang[lang_len] = '\0';
                        }
                        code_start = scan + 1;
                        while (*code_start == '\n') code_start++;
                        break;
                    }
                    scan++;
                }

                if (code_start) {
                    code_end = code_start;
                    while (*code_end) {
                        if (code_end[0] == '`' && code_end[1] == '`' && code_end[2] == '`') {
                            while (code_end > code_start && (code_end[-1] == '\n' || code_end[-1] == '`')) code_end--;
                            break;
                        }
                        code_end++;
                    }

                    if (code_end > code_start) {
                        block_counter++;
                        attron(A_REVERSE);
                        mvprintw(row++, 0, "  [Code Block %d]%s%s",
                            block_counter,
                            lang[0] ? " (" : "",
                            lang[0] ? lang : "");
                        clrtoeol();
                        attroff(A_REVERSE);
                        
                        char line_num[16];
                        int line_num_width = 6;
                        const char *cp = code_start;
                        int line_num_val = 1;
                        
                        attron(A_DIM);
                        mvaddstr(row++, 0, "  +");
                        for (int i = 0; i < COLS - 4; i++) addch('-');
                        attroff(A_DIM);
                        
                        while (*cp && cp < code_end && row < display_rows - 1) {
                            snprintf(line_num, sizeof(line_num), "%4d|", line_num_val);
                            attron(A_DIM);
                            mvaddstr(row, 0, line_num);
                            attroff(A_DIM);
                            
                            int col = line_num_width;
                            move(row, col);
                            
                            while (*cp && *cp != '\n' && col < COLS - 1) {
                                addch(*cp++);
                                col++;
                            }
                            
                            if (*cp == '\n') cp++;
                            line_num_val++;
                            row++;
                        }
                        
                        attron(A_DIM);
                        mvaddstr(row++, 0, "  +");
                        for (int i = 0; i < COLS - 4; i++) addch('-');
                        attroff(A_DIM);
                        
                        p = code_end + 3;
                        while (*p == '\n') p++;
                        continue;
                    }
                }
                p++;
            } else {
                if (*p == '\n') {
                    int nl_count = 0;
                    while (*p == '\n' && nl_count < 2) {
                        nl_count++;
                        p++;
                    }
                    if (*p != '\0') {
                        row++;
                    }
                } else {
                    addch(*p++);
                }
            }
        }
        row++;
    }

    move(LINES - 2, 0);
    attron(A_REVERSE);
    addstr(">>> ");
    int max_len = COLS - 8;
    int buf_len = strlen(input_buf);
    int start = 0;
    if (buf_len > max_len) {
        start = buf_len - max_len;
        addstr("...");
    }
    for (int i = start; i < buf_len; i++) {
        addch(input_buf[i]);
    }
    clrtoeol();
    attroff(A_REVERSE);

    move(LINES - 1, 0);
    clrtoeol();
    mvaddstr(LINES - 1, 0, " ,c=paste code | C-y=paste | C-w=delete word | Esc=close");
    refresh();
}

static int prompt_code_block_number(void)
{
    char buf[32];
    mvaddstr(LINES - 1, 0, "Code block number: ");
    clrtoeol();
    refresh();

    int c, pos = 0;
    buf[0] = '\0';

    for (;;) {
        c = getch();
        if (c == 0x0d || c == 0x0a) {
            buf[pos] = '\0';
            break;
        }
        if (c == 0x07) return -1;
        if (c == 0x7f || c == 0x08) {
            if (pos > 0) {
                pos--;
                mvaddch(LINES - 1, 18 + pos, ' ');
                move(LINES - 1, 18 + pos);
                clrtoeol();
            }
            continue;
        }
        if (c >= '0' && c <= '9' && pos < 10) {
            buf[pos++] = c;
            addch(c);
        }
        refresh();
    }

    if (pos == 0) return -1;
    return atoi(buf);
}

static int prompt_line_number(int max_line)
{
    char buf[32];
    mvaddstr(LINES - 1, 0, "Line number (1-");
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", max_line);
    addstr(tmp);
    addstr("): ");
    clrtoeol();
    refresh();

    int c, pos = 0;
    buf[0] = '\0';

    for (;;) {
        c = getch();
        if (c == 0x0d || c == 0x0a) {
            buf[pos] = '\0';
            break;
        }
        if (c == 0x07) return -1;
        if (c == 0x7f || c == 0x08) {
            if (pos > 0) {
                pos--;
                int start = 20 + strlen(tmp) + 3;
                mvaddch(LINES - 1, start + pos, ' ');
                move(LINES - 1, start + pos);
                clrtoeol();
            }
            continue;
        }
        if (c >= '0' && c <= '9' && pos < 10) {
            buf[pos++] = c;
            addch(c);
        }
        refresh();
    }

    if (pos == 0) return -1;
    return atoi(buf);
}

void ollama_chat_view(void)
{
    int c;
    int running = 1;
    int comma_pending = 0;

    nmessages = 0;
    input_pos = 0;
    ncode_blocks = 0;
    scroll_offset = 0;
    input_buf[0] = '\0';
    memset(messages, 0, sizeof(messages));
    memset(code_blocks, 0, sizeof(code_blocks));
    strncpy(model, DEFAULT_MODEL, sizeof(model) - 1);
    model[sizeof(model) - 1] = '\0';

    curl_global_init(CURL_GLOBAL_DEFAULT);

    draw_ollama_view();

    while (running) {
        c = getch();

        if (comma_pending) {
            if (c == 'c' || c == 'C') {
                for (int i = 0; i < nmessages; i++) {
                    if (!messages[i].is_user) {
                        parse_code_blocks(messages[i].content);
                    }
                }

                if (ncode_blocks == 0) {
                    mvaddstr(LINES - 1, 0, "No code blocks in conversation.");
                    refresh();
                    timeout(1500);
                    getch();
                    timeout(-1);
                    comma_pending = 0;
                    draw_ollama_view();
                    continue;
                }

                int block_num = prompt_code_block_number();
                if (block_num < 0 || block_num < 1 || block_num > ncode_blocks) {
                    draw_ollama_view();
                    comma_pending = 0;
                    continue;
                }

                int curline, lastline;
                get_line_stats(&curline, &lastline);

                int line_num = prompt_line_number(lastline);
                if (line_num < 0) {
                    draw_ollama_view();
                    comma_pending = 0;
                    continue;
                }

                curl_global_cleanup();
                endwin();
                reset_prog_mode();
                refresh();

                if (insert_code_block_at_line(block_num - 1, line_num)) {
                    msg("Inserted code block %d at line %d", block_num, line_num);
                } else {
                    msg("Failed to insert code block");
                }

                refresh();
                def_prog_mode();
                endwin();
                curl_global_init(CURL_GLOBAL_DEFAULT);

                draw_ollama_view();
                comma_pending = 0;
                continue;
            } else {
                if (input_pos < INPUT_BUF - 1) {
                    input_buf[input_pos++] = ',';
                    input_buf[input_pos] = '\0';
                }
                comma_pending = 0;
            }
        }

        if (c == ',') {
            comma_pending = 1;
            draw_ollama_view();
            continue;
        }

        if (c == 0x1b) {
            timeout(50);
            int c2 = getch();
            timeout(-1);
            if (c2 == ERR) {
                running = 0;
                continue;
            }
            if (c2 == 0x5b) {
                int c3 = getch();
                if (c3 == 0x35) {
                    getch();
                    scroll_offset -= (LINES - 4) / 2;
                    if (scroll_offset < 0) scroll_offset = 0;
                    draw_ollama_view();
                    continue;
                }
                if (c3 == 0x36) {
                    getch();
                    scroll_offset += (LINES - 4) / 2;
                    if (scroll_offset >= nmessages) scroll_offset = nmessages - 1;
                    if (scroll_offset < 0) scroll_offset = 0;
                    draw_ollama_view();
                    continue;
                }
                if (c3 >= 'A' && c3 <= 'D') {
                    if (c3 == 'A') {
                        scroll_offset -= (LINES - 4) / 2;
                        if (scroll_offset < 0) scroll_offset = 0;
                        draw_ollama_view();
                    }
                    continue;
                }
            }
            continue;
        }

        switch (c) {
        case 0x07:
            running = 0;
            break;
        case '\n':
        case '\r':
            if (input_pos > 0) {
                input_buf[input_pos] = '\0';
                add_user_message(input_buf);
                scroll_offset = 0;
                draw_ollama_view();

                mvaddstr(LINES - 1, 0, "Thinking...                                   ");
                refresh();

                if (ollama_chat(input_buf) != 0) {
                    mvaddstr(LINES - 1, 0, "Error: Could not connect to Ollama. Is ollama running?");
                    refresh();
                    timeout(2000);
                    getch();
                    timeout(-1);
                }

                for (int i = 0; i < nmessages; i++) {
                    if (!messages[i].is_user) {
                        parse_code_blocks(messages[i].content);
                    }
                }

                input_pos = 0;
                input_buf[0] = '\0';
            }
            break;
        case 0x17:
            if (input_pos > 0) {
                while (input_pos > 0 && (input_buf[input_pos - 1] & 0xC0) != 0x80) {
                    input_pos--;
                }
                input_buf[input_pos] = '\0';
            }
            break;
        case 0x7f:
        case 0x08:
            if (input_pos > 0) {
                input_pos--;
                input_buf[input_pos] = '\0';
            }
            break;
        case 0x19:
            if (scrap && nscrap > 0 && input_pos + nscrap < INPUT_BUF - 1) {
                for (int i = 0; i < nscrap && input_pos < INPUT_BUF - 1; i++) {
                    input_buf[input_pos++] = scrap[i];
                }
                input_buf[input_pos] = '\0';
            }
            break;
        default:
            if (c >= 32 && c < 127 && input_pos < INPUT_BUF - 2) {
                input_buf[input_pos++] = c;
                input_buf[input_pos] = '\0';
            }
            break;
        }

        draw_ollama_view();
    }

    curl_global_cleanup();
    redraw();
}
