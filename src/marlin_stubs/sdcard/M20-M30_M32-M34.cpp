#include <dirent.h>
#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../lib/Marlin/Marlin/src/gcode/gcode.h"
#include "../src/common/print_utils.hpp"
#include "marlin_server.hpp"
#include <usb_host.h>
#include "marlin_vars.hpp"
#include <common/directory.hpp>
#include <common/filepath_operation.h>
#include <common/filename_type.hpp>
#include <common/gcode/gcode_reader_restore_info.hpp>
#include <common/path_utils.h>

namespace {

constexpr const char *media_root = "/usb/";
constexpr size_t media_root_len = 5;
constexpr uint8_t max_list_depth = 4;

char selected_media_path[FILE_PATH_BUFFER_LEN] = {};
uint32_t selected_media_offset = 0;
FILE *upload_file = nullptr;

bool media_ready() {
    return marlin_vars().media_inserted.get();
}

bool is_hidden_or_parent(const char *name) {
    return name[0] == '.';
}

bool join_path(char *out, size_t out_size, const char *base, const char *name) {
    const size_t base_len = strlcpy(out, base, out_size);
    if (base_len >= out_size) {
        return false;
    }

    if (base_len && out[base_len - 1] != '/' && strlcat(out, "/", out_size) >= out_size) {
        return false;
    }

    return strlcat(out, name, out_size) < out_size;
}

bool build_media_path(const char *arg, char *out, size_t out_size) {
    if (!arg || !arg[0]) {
        return false;
    }

    while (*arg == ' ') {
        ++arg;
    }

    const char *path = arg;
    if (strncmp(path, media_root, media_root_len) == 0) {
        path += media_root_len;
    }
    while (*path == '/') {
        ++path;
    }

    if (!path[0] || strstr(path, "..")) {
        return false;
    }

    return join_path(out, out_size, media_root, path);
}

const char *media_relative_path(const char *path) {
    return strncmp(path, media_root, media_root_len) == 0 ? path + media_root_len : path;
}

uint32_t file_size_or_zero(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 ? static_cast<uint32_t>(st.st_size) : 0;
}

void print_file_entry(const char *path, const char *long_name, bool include_lfn) {
    SERIAL_ECHO(media_relative_path(path));
    SERIAL_CHAR(' ');
    SERIAL_ECHO(file_size_or_zero(path));
    if (include_lfn) {
        SERIAL_CHAR(' ');
        SERIAL_ECHO(long_name);
    }
    SERIAL_EOL();
}

void list_dir_recursive(const char *dir_path, const char *relative_dir, bool include_lfn, uint8_t depth) {
    if (depth > max_list_depth) {
        return;
    }

    Directory dir { dir_path };
    if (!dir) {
        return;
    }

    while (struct dirent *entry = dir.read()) {
        if (!entry->d_name[0] || is_hidden_or_parent(entry->d_name)) {
            continue;
        }

        std::array<char, FILE_PATH_BUFFER_LEN> full_path {};
        if (!join_path(full_path.data(), full_path.size(), dir_path, entry->d_name)) {
            continue;
        }

        if (entry->d_type == DT_DIR) {
            std::array<char, FILE_PATH_BUFFER_LEN> relative_path {};
            if (!join_path(relative_path.data(), relative_path.size(), relative_dir, entry->d_name)) {
                continue;
            }
            list_dir_recursive(full_path.data(), relative_path.data(), include_lfn, depth + 1);
        } else if (entry->d_type == DT_REG && filename_is_printable(entry->d_name)) {
            std::array<char, FILE_PATH_BUFFER_LEN> relative_lfn {};
            const char *long_name = entry->d_name;
            if (include_lfn) {
                if (!join_path(relative_lfn.data(), relative_lfn.size(), relative_dir, dirent_lfn(entry))) {
                    continue;
                }
                long_name = relative_lfn.data();
            }
            print_file_entry(full_path.data(), long_name, include_lfn);
        }
    }
}

void upload_write_line(const char *command) {
    if (!upload_file) {
        return;
    }

    const char *line = command;
    if (*line == 'N') {
        const char *end = line + 1;
        while (*end >= '0' && *end <= '9') {
            ++end;
        }
        if (*end == ' ') {
            line = end + 1;
        }
    }

    while (*line && *line != '*') {
        fputc(*line++, upload_file);
    }

    fputc('\n', upload_file);
}

const char *arg_after_command(const char *command, uint16_t code) {
    const char *cursor = command;
    while (*cursor == ' ') {
        ++cursor;
    }
    if (*cursor == 'N') {
        ++cursor;
        while (*cursor == '-' || (*cursor >= '0' && *cursor <= '9')) {
            ++cursor;
        }
        while (*cursor == ' ') {
            ++cursor;
        }
    }

    if (*cursor != 'M') {
        return nullptr;
    }
    ++cursor;

    char *end = nullptr;
    const long parsed = strtol(cursor, &end, 10);
    if (end == cursor || parsed != code || (*end != '\0' && *end != ' ' && *end != '*')) {
        return nullptr;
    }

    while (*end == ' ') {
        ++end;
    }

    static char arg_buffer[FILE_PATH_BUFFER_LEN] = {};
    size_t i = 0;
    while (*end && *end != '*' && i + 1 < sizeof(arg_buffer)) {
        arg_buffer[i++] = *end++;
    }
    arg_buffer[i] = '\0';
    return arg_buffer;
}

bool open_upload_path(const char *path_arg) {
    if (!media_ready()) {
        SERIAL_ECHOLNPGM(MSG_SD_CARD_FAIL);
        return false;
    }

    std::array<char, FILE_PATH_BUFFER_LEN> filepath {};
    if (!build_media_path(path_arg, filepath.data(), filepath.size()) || !make_dirs(filepath.data())) {
        SERIAL_ECHOLNPGM(MSG_SD_OPEN_FILE_FAIL);
        return false;
    }

    if (upload_file) {
        fclose(upload_file);
        upload_file = nullptr;
    }

    upload_file = fopen(filepath.data(), "wb");
    if (!upload_file) {
        SERIAL_ECHOLNPGM(MSG_SD_OPEN_FILE_FAIL);
        return false;
    }

    SERIAL_ECHOPGM(MSG_SD_WRITE_TO_FILE);
    SERIAL_ECHOLN(media_relative_path(filepath.data()));
    return true;
}

void close_upload() {
    if (upload_file) {
        fclose(upload_file);
        upload_file = nullptr;
    }
    SERIAL_ECHOLNPGM("Done saving file.");
}

} // namespace

extern "C" bool buddy_sdcard_upload_active() {
    return upload_file != nullptr;
}

extern "C" void buddy_sdcard_upload_handle_line(const char *command) {
    upload_write_line(command);
}

extern "C" bool buddy_sdcard_upload_start_command(const char *command) {
    const char *path_arg = arg_after_command(command, 28);
    return path_arg != nullptr && open_upload_path(path_arg);
}

extern "C" void buddy_sdcard_upload_finish_command() {
    close_upload();
}

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M20 - List SD card on serial port  <a href="https://reprap.org/wiki/G-code#M20:_List_SD_card">M20: List SD card</a>
 *
 *#### Usage
 *
 *    M20
 *
 */
void GcodeSuite::M20() {
    SERIAL_ECHOLNPGM(MSG_BEGIN_FILE_LIST);
    if (media_ready()) {
        list_dir_recursive(media_root, "", parser.seen('L'), 0);
    }
    SERIAL_ECHOLNPGM(MSG_END_FILE_LIST);
}

/**
 *### M21 - Initialize SD card <a href="https://reprap.org/wiki/G-code#M21:_Initialize_SD_card">M21: Initialize SD card</a>
 *
 *#### Usage
 *
 *    M21
 *
 */
void GcodeSuite::M21() {
    // required for Octoprint / third party tools to simulate an inserted SD card when using USB
    if (marlin_vars().media_inserted.get()) {
        SERIAL_ECHOLNPGM(MSG_SD_CARD_OK);
    } else {
        SERIAL_ECHOLNPGM(MSG_SD_CARD_FAIL);
    }
}

/**
 *### M22: Release SD card <a href="https://reprap.org/wiki/G-code#M22:_Release_SD_card">M22: Release SD card</a>
 *
 *#### Usage
 *
 *    M22
 *
 */
void GcodeSuite::M22() {
    // Nothing to release for USB-backed media.
}

/**
 *### M23 - Select SD file <a href="https://reprap.org/wiki/G-code#M23:_Select_SD_file">M23: Select SD file</a>
 *
 *#### Usage
 *
 *    M23 [ filename ]
 *
 */
void GcodeSuite::M23() {
    if (!media_ready() || !build_media_path(parser.string_arg, selected_media_path, sizeof(selected_media_path)) || !file_exists(selected_media_path)) {
        SERIAL_ECHOLNPGM("open failed, File: ");
        selected_media_path[0] = '\0';
        return;
    }

    selected_media_offset = 0;
    marlin_vars().media_SFN_path.set(selected_media_path);

    SERIAL_ECHOPGM(MSG_SD_FILE_OPENED);
    SERIAL_ECHO(media_relative_path(selected_media_path));
    SERIAL_ECHOPGM(MSG_SD_SIZE);
    SERIAL_ECHOLN(file_size_or_zero(selected_media_path));
    // Do not remove. Used by third party tools to detect that a file has been selected
    SERIAL_ECHOLNPGM(MSG_SD_FILE_SELECTED);
}

/**
 *### M24 - Start/resume SD print <a href="https://reprap.org/wiki/G-code#M24:_Start.2Fresume_SD_print">M24: Start/resume SD print</a>
 *
 *#### Usage
 *
 *    M24
 *
 */
void GcodeSuite::M24() {
    const auto print_state = marlin_vars().print_state.get();
    if (marlin_server::is_printing_state(print_state) || marlin_server::is_extended_paused_state(print_state)) {
        marlin_server::print_resume();
    } else if (selected_media_path[0]) {
        GCodeReaderPosition resume_pos;
        resume_pos.offset = selected_media_offset;
        marlin_server::print_start(selected_media_path, resume_pos, marlin_server::PreviewSkipIfAble::all);
    } else {
        SERIAL_ECHOLNPGM(MSG_SD_NOT_PRINTING);
    }
}

/**
 *### Pause SD print <a href="https://reprap.org/wiki/G-code#M25:_Pause_SD_print">M25: Pause SD print</a>
 *
 *#### Usage
 *
 *    M25
 *
 */
void GcodeSuite::M25() {
    marlin_server::print_pause();
}

/**
 *### M26 - Set SD position <a href="https://reprap.org/wiki/G-code#M26:_Set_SD_position">M26: Set SD position</a>
 *
 *#### Usage
 *
 *    M26 [ S ]
 *
 *
 *#### Parameters
 *
 *  - `S` - Specific position
 *
 */
void GcodeSuite::M26() {
    if (usb_host::is_media_inserted() && parser.seenval('S')) {
        selected_media_offset = parser.value_ulong();
        marlin_server::set_media_position(selected_media_offset);
    }
}

/**
 *### M27 - Report SD print status on serial port <a href="https://reprap.org/wiki/G-code#M27:_Report_SD_print_status">M27: Report SD print status</a>
 *
 *#### Usage
 *
 *    M37 [ C ]
 *
 *#### Parameters
 *
 *  - `C` - Report current file's short file name instead
 *
 */
void GcodeSuite::M27() {
    if (marlin_server::is_printing_state(marlin_vars().print_state.get())) {
        SERIAL_ECHOPGM(MSG_SD_PRINTING_BYTE);
        SERIAL_ECHO(marlin_vars().media_position.get());
        SERIAL_CHAR('/');
        SERIAL_ECHOLN(marlin_vars().media_size_estimate.get());
    } else {
        SERIAL_ECHOLNPGM(MSG_SD_NOT_PRINTING);
    }
}

/**
 *### M32 - Select file and start SD print <a href="https://reprap.org/wiki/G-code#M32:_Select_file_and_start_SD_print">M32: Select file and start SD print</a>
 *
 *#### Usage
 *
 *    M32 [ filename ]
 *
 */
void GcodeSuite::M32() {
    M23();
    M24();
}

/** @}*/

/**
 *### M28 - Start SD write <a href="https://reprap.org/wiki/G-code#M28:_Begin_write_to_SD_card">M28: Begin write to SD card</a>
 *
 *#### Usage
 *
 *   M28 []
 */
void GcodeSuite::M28() {
    open_upload_path(parser.string_arg);
}

/**
 *### M29 - Stop SD write <a href="https://reprap.org/wiki/G-code#M29:_Stop_writing_to_SD_card">M29: Stop writing to SD card</a>
 *
 *Stops writing to the SD file signaling the end of the uploaded file. It is processed very early and it's not written to the card.
 *
 *#### Usage
 *
 *   M29 []
 */
void GcodeSuite::M29() {
    close_upload();
}

/**
 *### M30 - Delete file <a href="https://reprap.org/wiki/G-code#M30:_Delete_a_file_on_the_SD_card">M30: Delete a file on the SD card</a>
 *
 *#### Usage
 *
 *    M30 [filename]
 */
void GcodeSuite::M30() {
    std::array<char, FILE_PATH_BUFFER_LEN> full_path {};
    DeleteResult result = build_media_path(parser.string_arg, full_path.data(), full_path.size())
        ? remove_file(full_path.data())
        : DeleteResult::GeneralError;
    SERIAL_ECHOPGM(result == DeleteResult::Success ? "File deleted:" : "Deletion failed:");
    SERIAL_ECHO(parser.string_arg);
    SERIAL_ECHOLN(".");
}

//
// ### M33 - Get the long name for an SD card file or folder
// ### M33 - Stop and Close File and save restart.gcode
// ### M34 - Set SD file sorting options
