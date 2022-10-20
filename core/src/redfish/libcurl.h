/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file libcurl.h
 */

#pragma once

#include <cstddef>
#include <dlfcn.h>
#include <link.h>
#include <sstream>
#include <vector>
#include <iostream>

typedef void CURL;

#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_OFF_T         30000

#define CURLOPTTYPE_STRINGPOINT  CURLOPTTYPE_OBJECTPOINT
#define CURLOPTTYPE_SLISTPOINT  CURLOPTTYPE_OBJECTPOINT

#define CINIT(na,t,nu) CURLOPT_ ## na = CURLOPTTYPE_ ## t + nu

typedef enum {
    CINIT(TIMEOUT, LONG, 13),
    CINIT(VERBOSE, LONG, 41),
    CINIT(WRITEDATA, OBJECTPOINT, 1),
    CINIT(URL, STRINGPOINT, 2),
    CINIT(HTTPHEADER, SLISTPOINT, 23),
    CINIT(CUSTOMREQUEST, STRINGPOINT, 36),
    CINIT(NOPROXY, STRINGPOINT, 177),
    CINIT(FOLLOWLOCATION, LONG, 52),
    CINIT(SSL_VERIFYPEER, LONG, 64),
    CINIT(SSL_VERIFYHOST, LONG, 81),
    CINIT(WRITEFUNCTION, FUNCTIONPOINT, 11),
    CINIT(HTTPAUTH, LONG, 107),
    CINIT(USERNAME, STRINGPOINT, 173),
    CINIT(PASSWORD, STRINGPOINT, 174),
    CINIT(MIMEPOST, OBJECTPOINT, 269),
} CURLoption;

typedef enum {
  CURLE_OK = 0,
  CURLE_UNSUPPORTED_PROTOCOL,    /* 1 */
  CURLE_FAILED_INIT,             /* 2 */
  CURLE_URL_MALFORMAT,           /* 3 */
  CURLE_NOT_BUILT_IN,            /* 4 - [was obsoleted in August 2007 for
                                    7.17.0, reused in April 2011 for 7.21.5] */
  CURLE_COULDNT_RESOLVE_PROXY,   /* 5 */
  CURLE_COULDNT_RESOLVE_HOST,    /* 6 */
  CURLE_COULDNT_CONNECT,         /* 7 */
  CURLE_WEIRD_SERVER_REPLY,      /* 8 */
  CURLE_REMOTE_ACCESS_DENIED,    /* 9 a service was denied by the server
                                    due to lack of access - when login fails
                                    this is not returned. */
  CURLE_FTP_ACCEPT_FAILED,       /* 10 - [was obsoleted in April 2006 for
                                    7.15.4, reused in Dec 2011 for 7.24.0]*/
  CURLE_FTP_WEIRD_PASS_REPLY,    /* 11 */
  CURLE_FTP_ACCEPT_TIMEOUT,      /* 12 - timeout occurred accepting server
                                    [was obsoleted in August 2007 for 7.17.0,
                                    reused in Dec 2011 for 7.24.0]*/
  CURLE_FTP_WEIRD_PASV_REPLY,    /* 13 */
  CURLE_FTP_WEIRD_227_FORMAT,    /* 14 */
  CURLE_FTP_CANT_GET_HOST,       /* 15 */
  CURLE_HTTP2,                   /* 16 - A problem in the http2 framing layer.
                                    [was obsoleted in August 2007 for 7.17.0,
                                    reused in July 2014 for 7.38.0] */
  CURLE_FTP_COULDNT_SET_TYPE,    /* 17 */
  CURLE_PARTIAL_FILE,            /* 18 */
  CURLE_FTP_COULDNT_RETR_FILE,   /* 19 */
  CURLE_OBSOLETE20,              /* 20 - NOT USED */
  CURLE_QUOTE_ERROR,             /* 21 - quote command failure */
  CURLE_HTTP_RETURNED_ERROR,     /* 22 */
  CURLE_WRITE_ERROR,             /* 23 */
  CURLE_OBSOLETE24,              /* 24 - NOT USED */
  CURLE_UPLOAD_FAILED,           /* 25 - failed upload "command" */
  CURLE_READ_ERROR,              /* 26 - couldn't open/read from file */
  CURLE_OUT_OF_MEMORY,           /* 27 */
  /* Note: CURLE_OUT_OF_MEMORY may sometimes indicate a conversion error
           instead of a memory allocation error if CURL_DOES_CONVERSIONS
           is defined
  */
  CURLE_OPERATION_TIMEDOUT,      /* 28 - the timeout time was reached */
  CURLE_OBSOLETE29,              /* 29 - NOT USED */
  CURLE_FTP_PORT_FAILED,         /* 30 - FTP PORT operation failed */
  CURLE_FTP_COULDNT_USE_REST,    /* 31 - the REST command failed */
  CURLE_OBSOLETE32,              /* 32 - NOT USED */
  CURLE_RANGE_ERROR,             /* 33 - RANGE "command" didn't work */
  CURLE_HTTP_POST_ERROR,         /* 34 */
  CURLE_SSL_CONNECT_ERROR,       /* 35 - wrong when connecting with SSL */
  CURLE_BAD_DOWNLOAD_RESUME,     /* 36 - couldn't resume download */
  CURLE_FILE_COULDNT_READ_FILE,  /* 37 */
  CURLE_LDAP_CANNOT_BIND,        /* 38 */
  CURLE_LDAP_SEARCH_FAILED,      /* 39 */
  CURLE_OBSOLETE40,              /* 40 - NOT USED */
  CURLE_FUNCTION_NOT_FOUND,      /* 41 - NOT USED starting with 7.53.0 */
  CURLE_ABORTED_BY_CALLBACK,     /* 42 */
  CURLE_BAD_FUNCTION_ARGUMENT,   /* 43 */
  CURLE_OBSOLETE44,              /* 44 - NOT USED */
  CURLE_INTERFACE_FAILED,        /* 45 - CURLOPT_INTERFACE failed */
  CURLE_OBSOLETE46,              /* 46 - NOT USED */
  CURLE_TOO_MANY_REDIRECTS,      /* 47 - catch endless re-direct loops */
  CURLE_UNKNOWN_OPTION,          /* 48 - User specified an unknown option */
  CURLE_TELNET_OPTION_SYNTAX,    /* 49 - Malformed telnet option */
  CURLE_OBSOLETE50,              /* 50 - NOT USED */
  CURLE_OBSOLETE51,              /* 51 - NOT USED */
  CURLE_GOT_NOTHING,             /* 52 - when this is a specific error */
  CURLE_SSL_ENGINE_NOTFOUND,     /* 53 - SSL crypto engine not found */
  CURLE_SSL_ENGINE_SETFAILED,    /* 54 - can not set SSL crypto engine as
                                    default */
  CURLE_SEND_ERROR,              /* 55 - failed sending network data */
  CURLE_RECV_ERROR,              /* 56 - failure in receiving network data */
  CURLE_OBSOLETE57,              /* 57 - NOT IN USE */
  CURLE_SSL_CERTPROBLEM,         /* 58 - problem with the local certificate */
  CURLE_SSL_CIPHER,              /* 59 - couldn't use specified cipher */
  CURLE_PEER_FAILED_VERIFICATION, /* 60 - peer's certificate or fingerprint
                                     wasn't verified fine */
  CURLE_BAD_CONTENT_ENCODING,    /* 61 - Unrecognized/bad encoding */
  CURLE_LDAP_INVALID_URL,        /* 62 - Invalid LDAP URL */
  CURLE_FILESIZE_EXCEEDED,       /* 63 - Maximum file size exceeded */
  CURLE_USE_SSL_FAILED,          /* 64 - Requested FTP SSL level failed */
  CURLE_SEND_FAIL_REWIND,        /* 65 - Sending the data requires a rewind
                                    that failed */
  CURLE_SSL_ENGINE_INITFAILED,   /* 66 - failed to initialise ENGINE */
  CURLE_LOGIN_DENIED,            /* 67 - user, password or similar was not
                                    accepted and we failed to login */
  CURLE_TFTP_NOTFOUND,           /* 68 - file not found on server */
  CURLE_TFTP_PERM,               /* 69 - permission problem on server */
  CURLE_REMOTE_DISK_FULL,        /* 70 - out of disk space on server */
  CURLE_TFTP_ILLEGAL,            /* 71 - Illegal TFTP operation */
  CURLE_TFTP_UNKNOWNID,          /* 72 - Unknown transfer ID */
  CURLE_REMOTE_FILE_EXISTS,      /* 73 - File already exists */
  CURLE_TFTP_NOSUCHUSER,         /* 74 - No such user */
  CURLE_CONV_FAILED,             /* 75 - conversion failed */
  CURLE_CONV_REQD,               /* 76 - caller must register conversion
                                    callbacks using curl_easy_setopt options
                                    CURLOPT_CONV_FROM_NETWORK_FUNCTION,
                                    CURLOPT_CONV_TO_NETWORK_FUNCTION, and
                                    CURLOPT_CONV_FROM_UTF8_FUNCTION */
  CURLE_SSL_CACERT_BADFILE,      /* 77 - could not load CACERT file, missing
                                    or wrong format */
  CURLE_REMOTE_FILE_NOT_FOUND,   /* 78 - remote file not found */
  CURLE_SSH,                     /* 79 - error from the SSH layer, somewhat
                                    generic so the error message will be of
                                    interest when this has happened */

  CURLE_SSL_SHUTDOWN_FAILED,     /* 80 - Failed to shut down the SSL
                                    connection */
  CURLE_AGAIN,                   /* 81 - socket is not ready for send/recv,
                                    wait till it's ready and try again (Added
                                    in 7.18.2) */
  CURLE_SSL_CRL_BADFILE,         /* 82 - could not load CRL file, missing or
                                    wrong format (Added in 7.19.0) */
  CURLE_SSL_ISSUER_ERROR,        /* 83 - Issuer check failed.  (Added in
                                    7.19.0) */
  CURLE_FTP_PRET_FAILED,         /* 84 - a PRET command failed */
  CURLE_RTSP_CSEQ_ERROR,         /* 85 - mismatch of RTSP CSeq numbers */
  CURLE_RTSP_SESSION_ERROR,      /* 86 - mismatch of RTSP Session Ids */
  CURLE_FTP_BAD_FILE_LIST,       /* 87 - unable to parse FTP file list */
  CURLE_CHUNK_FAILED,            /* 88 - chunk callback reported error */
  CURLE_NO_CONNECTION_AVAILABLE, /* 89 - No connection available, the
                                    session will be queued */
  CURLE_SSL_PINNEDPUBKEYNOTMATCH, /* 90 - specified pinned public key did not
                                     match */
  CURLE_SSL_INVALIDCERTSTATUS,   /* 91 - invalid certificate status */
  CURLE_HTTP2_STREAM,            /* 92 - stream error in HTTP/2 framing layer
                                    */
  CURLE_RECURSIVE_API_CALL,      /* 93 - an api function was called from
                                    inside a callback */
  CURLE_AUTH_ERROR,              /* 94 - an authentication function returned an
                                    error */
  CURLE_HTTP3,                   /* 95 - An HTTP/3 layer problem */
  CURL_LAST /* never use! */
} CURLcode;


typedef struct curl_mime      curl_mime;
typedef struct curl_mimepart  curl_mimepart;

#define CURLAUTH_BASIC        (((unsigned long)1)<<0)

#define CURL_ZERO_TERMINATED ((size_t) -1)

struct curl_slist {
  char *data;
  struct curl_slist *next;
};

typedef enum {
  CURLVERSION_FIRST,
  CURLVERSION_SECOND,
  CURLVERSION_THIRD,
  CURLVERSION_FOURTH,
  CURLVERSION_FIFTH,
  CURLVERSION_SIXTH,
  CURLVERSION_SEVENTH,
  CURLVERSION_EIGHTH,
  CURLVERSION_NINTH,
  CURLVERSION_TENTH,
  CURLVERSION_LAST /* never actually use this */
} CURLversion;

struct curl_version_info_data {
  CURLversion age;          /* age of the returned struct */
  const char *version;      /* LIBCURL_VERSION */
  unsigned int version_num; /* LIBCURL_VERSION_NUM */
  const char *host;         /* OS/host/cpu/machine when configured */
  int features;             /* bitmask, see defines below */
  const char *ssl_version;  /* human readable string */
  long ssl_version_num;     /* not used anymore, always 0 */
  const char *libz_version; /* human readable string */
  /* protocols is terminated by an entry with a NULL protoname */
  const char * const *protocols;

  /* The fields below this were added in CURLVERSION_SECOND */
  const char *ares;
  int ares_num;

  /* This field was added in CURLVERSION_THIRD */
  const char *libidn;

  /* These field were added in CURLVERSION_FOURTH */

  /* Same as '_libiconv_version' if built with HAVE_ICONV */
  int iconv_ver_num;

  const char *libssh_version; /* human readable string */

  /* These fields were added in CURLVERSION_FIFTH */
  unsigned int brotli_ver_num; /* Numeric Brotli version
                                  (MAJOR << 24) | (MINOR << 12) | PATCH */
  const char *brotli_version; /* human readable string. */

  /* These fields were added in CURLVERSION_SIXTH */
  unsigned int nghttp2_ver_num; /* Numeric nghttp2 version
                                   (MAJOR << 16) | (MINOR << 8) | PATCH */
  const char *nghttp2_version; /* human readable string. */
  const char *quic_version;    /* human readable quic (+ HTTP/3) library +
                                  version or NULL */

  /* These fields were added in CURLVERSION_SEVENTH */
  const char *cainfo;          /* the built-in default CURLOPT_CAINFO, might
                                  be NULL */
  const char *capath;          /* the built-in default CURLOPT_CAPATH, might
                                  be NULL */

  /* These fields were added in CURLVERSION_EIGHTH */
  unsigned int zstd_ver_num; /* Numeric Zstd version
                                  (MAJOR << 24) | (MINOR << 12) | PATCH */
  const char *zstd_version; /* human readable string. */

  /* These fields were added in CURLVERSION_NINTH */
  const char *hyper_version; /* human readable string. */

  /* These fields were added in CURLVERSION_TENTH */
  const char *gsasl_version; /* human readable string. */
};
typedef struct curl_version_info_data curl_version_info_data;

typedef CURL *(*curl_easy_init_t)(void);
typedef CURLcode (*curl_easy_setopt_t)(CURL *curl, CURLoption option, ...);
typedef CURLcode (*curl_easy_perform_t)(CURL *curl);
typedef void (*curl_easy_cleanup_t)(CURL *curl);
typedef curl_mime *(*curl_mime_init_t)(CURL *easy);
typedef curl_mimepart *(*curl_mime_addpart_t)(curl_mime *mime);
typedef CURLcode (*curl_mime_name_t)(curl_mimepart *part, const char *name);
typedef CURLcode (*curl_mime_type_t)(curl_mimepart *part, const char *mimetype);
typedef CURLcode (*curl_mime_data_t)(curl_mimepart *part, const char *data, size_t datasize);
typedef CURLcode (*curl_mime_filedata_t)(curl_mimepart *part, const char *filename);
typedef struct curl_slist *(*curl_slist_append_t)(struct curl_slist *, const char *);
typedef curl_version_info_data *(*curl_version_info_t)(CURLversion age);

struct CurlLibVersion {
    std::string name;
    int major;
    int minor;
    int patch;
    bool valid;
    CurlLibVersion(std::string str) {
        valid = true;
        name = str;
        major = minor = patch = 0;
        int dotNum = 0;
        try {
            std::string versionPart = name.substr(10);
            for (char c : versionPart) {
                if (c == '.')
                    dotNum++;
            }
            if (dotNum == 0)
                return;
            versionPart = versionPart.substr(1);
            std::istringstream iss;
            iss.str(versionPart);
            std::string buffer;

            if (std::getline(iss, buffer, '.')) {
                major = std::stoi(buffer);
            } else {
                return;
            }
            if (std::getline(iss, buffer, '.')) {
                minor = std::stoi(buffer);
            } else {
                return;
            }
            if (std::getline(iss, buffer, '.')) {
                patch = std::stoi(buffer);
            }
        } catch (...) {
            valid = false;
        }
    }
    bool operator<(const CurlLibVersion &other) {
        if (major < other.major)
            return true;
        if (major > other.major)
            return false;
        if (minor < other.minor)
            return true;
        if (minor > other.minor)
            return false;
        if (patch < other.patch)
            return true;
        else
            return false;
    }
};

std::string getLibCurlPath() {
    std::string cmd = "ldconfig -p 2>&1";
    FILE *f = popen(cmd.c_str(), "r");
    char c_line[1024];

    std::vector<CurlLibVersion> libList;

    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        auto idx = line.find("libcurl.so");
        if (idx != line.npos) {
            line = line.substr(idx);
            auto endIdx = line.find(' ');
            if (endIdx != line.npos) {
                std::string name = line.substr(0, endIdx);
                if (name.length() > 0) {
                    CurlLibVersion lib(name);
                    if (lib.valid)
                        libList.push_back(lib);
                }
            }
        }
    }
    pclose(f);
    if (libList.size() > 0) {
        auto &lib = libList.at(0);
        for (size_t i = 1; i < libList.size(); i++) {
            if (lib < libList.at(i)) {
                lib = libList.at(i);
            }
        }
        return lib.name;
    }
    return "libcurl.so";
}

class LibCurlApi {
   private:
    void *handle;
    std::string libPath = "Unknown";
    std::string initErrMsg;

   public:
    curl_easy_init_t curl_easy_init;
    curl_easy_setopt_t curl_easy_setopt;
    curl_easy_perform_t curl_easy_perform;
    curl_easy_cleanup_t curl_easy_cleanup;
    curl_mime_init_t curl_mime_init;
    curl_mime_addpart_t curl_mime_addpart;
    curl_mime_name_t curl_mime_name;
    curl_mime_type_t curl_mime_type;
    curl_mime_data_t curl_mime_data;
    curl_mime_filedata_t curl_mime_filedata;
    curl_slist_append_t curl_slist_append;
    curl_version_info_t curl_version_info;

   public:
    LibCurlApi() {
        handle = dlopen("libcurl.so", RTLD_LAZY);
        if(!handle){
            // can not find libcurl.so, try to find with version id
            std::string libCurlPath = getLibCurlPath();
            handle = dlopen(libCurlPath.c_str(), RTLD_LAZY);
        }
        if (!handle) {
            initErrMsg = "Fail to load libcurl.so. Please install libcurl.so with version equal to or higer than 7.56.0 first.";
            return;
        }
        struct link_map *p;
        int err = dlinfo(handle, RTLD_DI_LINKMAP, &p);
        if (err == 0 && p != NULL) {
            libPath = p->l_name;
        }
        curl_easy_init = reinterpret_cast<curl_easy_init_t>(dlsym(handle, "curl_easy_init"));
        curl_easy_setopt = reinterpret_cast<curl_easy_setopt_t>(dlsym(handle, "curl_easy_setopt"));
        curl_easy_perform = reinterpret_cast<curl_easy_perform_t>(dlsym(handle, "curl_easy_perform"));
        curl_easy_cleanup = reinterpret_cast<curl_easy_cleanup_t>(dlsym(handle, "curl_easy_cleanup"));
        curl_mime_init = reinterpret_cast<curl_mime_init_t>(dlsym(handle, "curl_mime_init"));
        curl_mime_addpart = reinterpret_cast<curl_mime_addpart_t>(dlsym(handle, "curl_mime_addpart"));
        curl_mime_name = reinterpret_cast<curl_mime_name_t>(dlsym(handle, "curl_mime_name"));
        curl_mime_type = reinterpret_cast<curl_mime_type_t>(dlsym(handle, "curl_mime_type"));
        curl_mime_data = reinterpret_cast<curl_mime_data_t>(dlsym(handle, "curl_mime_data"));
        curl_mime_filedata = reinterpret_cast<curl_mime_filedata_t>(dlsym(handle, "curl_mime_filedata"));
        curl_slist_append = reinterpret_cast<curl_slist_append_t>(dlsym(handle, "curl_slist_append"));
        curl_version_info = reinterpret_cast<curl_version_info_t>(dlsym(handle, "curl_version_info"));
        
        if (!initialized()) {
            if (!libPath.compare("Unknown")) {
                initErrMsg = "Fail to load " + libPath + ", please install libcurl.so with version equal to or higer than 7.56.0.";
            } else {
                initErrMsg = "Fail to load libcurl.so, please install libcurl.so with version equal to or higer than 7.56.0.";
            }
        }
    }
    ~LibCurlApi() {
        if (!handle)
            return;
        dlclose(handle);
    }
    bool initialized() {
        return handle != NULL &&
               curl_easy_init != NULL &&
               curl_easy_setopt != NULL &&
               curl_easy_perform != NULL &&
               curl_easy_cleanup != NULL &&
               curl_mime_init != NULL &&
               curl_mime_addpart != NULL &&
               curl_mime_name != NULL &&
               curl_mime_type != NULL &&
               curl_mime_data != NULL &&
               curl_mime_filedata != NULL &&
               curl_slist_append != NULL;
    }

    std::string getLibCurlVersion() {
        if (handle == NULL || curl_version_info == NULL)
            return "Unknown";
        auto version_data = curl_version_info(CURLVERSION_FIRST);
        return version_data->version;
    }

    std::string getLibPath() {
        return libPath;
    }

    std::string getInitErrMsg() {
        return initErrMsg;
    }
};