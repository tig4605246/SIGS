/***************************************
 *  Copyright (C) 2015 ThingWorx Inc.  *
 ***************************************/

/**
 * \file twAxTLS.h
 * \brief Portable ThingWorx AxTLS wrapper layer
*/

#ifndef AXTLS_H
#define AXTLS_H

#include "twOSPort.h"
#include "stringUtils.h"
#include "twLogger.h"

#include "stdio.h"
#include "string.h"

#if defined WIN32
#define STDCALL                 __stdcall
#endif
#include "ssl.h"

#ifdef __cplusplus
extern "C" {
#endif

int16_t returnValue(int16_t v);
typedef struct ssl_struct {
	SSL * ssl;
	uint8_t * leftover_buf;
	uint8_t * leftover_ptr;
	uint16_t leftover_cnt;
	uint16_t max_leftover;
	TW_MUTEX read_write_mutex;
} ssl_struct;

/* adding AxTls specific debug print statements	*/
/* they are very frequent and probably not very	*/
/* useful to most users so there is a seperate	*/
/* mechanism to print the logs					*/
#ifdef TW_SSL_TRACE
#define TW_AXTLS_LOG(fmt, ...)  twLog(TW_TRACE, fmt, ##__VA_ARGS__)
#else
#define TW_AXTLS_LOG(fmt, ...) 
#endif

/* #define to check the SSL_NEED_RECORD flag which is used to	*/
/* continue header reads after a frame timeout					*/
/* SSL_NEED_RECORD is currently defined within AxTls as 0x0001, but if that ever changes the below macro must change as well */
#define TW_AXTLS_IS_SET_SSL_FLAG		(ssl->ssl->flag & 0x0001)

#define TW_SSL_CTX						SSL_CTX
#define TW_SSL							ssl_struct
#define TW_SSL_SESSION_ID_SIZE			SSL_SESSION_ID_SIZE
#define TW_SSL_SESSION_ID(a)            a ? a->ssl->session_id : NULL
#define TW_GET_CERT_SIZE				ssl_get_config(SSL_MAX_CERT_CFG_OFFSET)
#define TW_GET_CA_CERT_SIZE				ssl_get_config(SSL_MAX_CA_CERT_CFG_OFFSET)
#define TW_NEW_SSL_CTX					ssl_ctx_new(options | SSL_SERVER_VERIFY_LATER, SSL_DEFAULT_CLNT_SESS)
#define TW_SSL_CTX_FREE(a)				ssl_ctx_free(a)
#define TW_USE_CERT_FILE(a,b,c)			ssl_obj_load(a, SSL_OBJ_X509_CERT, b, NULL)
#define TW_USE_KEY_FILE(a,b,c,d)		ssl_obj_load(a, SSL_OBJ_RSA_KEY, b, d)
#define TW_USE_CERT_CHAIN_FILE(a,b,c)	ssl_obj_load(a, SSL_OBJ_X509_CACERT, b, NULL)
#define TW_SET_CLIENT_CA_LIST(a,b)		ssl_obj_load(a, SSL_OBJ_X509_CACERT, (const char *)b, NULL)
#define TW_ENABLE_FIPS_MODE(a)			returnValue(TW_FIPS_MODE_NOT_SUPPORTED)
#define TW_SHA1_CTX						SHA1_CTX
#define TW_SHA1_INIT(a)					SHA1_Init(a)
#define TW_SHA1_UPDATE(a,b,c)			SHA1_Update(a,b,c)
#define TW_SHA1_FINAL(a,b)				SHA1_Final(a,b)
#define TW_MD5_CTX						MD5_CTX
#define TW_MD5_INIT(a)					MD5_Init(a)
#define TW_MD5_UPDATE(a,b,c)			MD5_Update(a,b,c)
#define TW_MD5_FINAL(a,b)				MD5_Final(a,b)
#define DATA_AVAILABLE(a,b,c)           1
 
static INLINE void TW_SSL_FREE(TW_SSL * s){
	if (s) {
		if (s->ssl) ssl_free(s->ssl);
		if (s->leftover_buf) TW_FREE(s->leftover_buf);
		if (s->read_write_mutex){
			twMutex_Unlock(s->read_write_mutex);
			twMutex_Delete(s->read_write_mutex);
		}
		TW_FREE(s);
	}
}

static INLINE TW_SSL * TW_NEW_SSL_CLIENT(SSL_CTX *ssl_ctx, twSocket * client_fd, void * session_id, uint8_t sess_id_size) {
	TW_SSL * s = (TW_SSL *)TW_CALLOC(sizeof(TW_SSL), 1);
	if (!s) return NULL;
	s->ssl = ssl_client_new(ssl_ctx, client_fd, (const uint8_t *)session_id, sess_id_size);
	if (!s->ssl) {
		TW_SSL_FREE(s);
		s = NULL;
	}

	s->read_write_mutex = twMutex_Create();
	if (!s->read_write_mutex){
		TW_SSL_FREE(s);
		s = NULL;
	}
	return s;
}

static INLINE char TW_HANDSHAKE_SUCCEEDED(TW_SSL * s) {
	if (!s || !s->ssl) return FALSE;
	return (ssl_handshake_status(s->ssl) == SSL_OK);
}

static INLINE TW_SSL * TW_NEW_SERVER(SSL_CTX *ssl_ctx, twSocket * client_fd) {
	TW_SSL * s = (TW_SSL *)TW_CALLOC(sizeof(TW_SSL), 1);
	if (!s) return NULL;
	s->ssl = ssl_server_new(ssl_ctx, client_fd);
	if (!s->ssl) {
		TW_SSL_FREE(s);
		s = NULL;
	}

	s->read_write_mutex = twMutex_Create();
	if (!s->read_write_mutex){
		TW_SSL_FREE(s);
		s = NULL;
	}
	return s;
}

/**
 * \brief Tries to read \p len bytes from the specified \p ssl into \p buf.
 *
 * \param[in]     ssl       The TLS/SSL connection to read from.
 * \param[out]    buf       A buffer to store the read data.
 * \param[in]     len       The number of bytes to read.
 * \param[in]     timeout   The amount of time (in milliseconds) to wait for
 *                          I/O to be unblocked.
 *
 * \return The number of bytes read or -1 if an error was encountered.
*/
static INLINE int TW_SSL_READ(TW_SSL * ssl, char * buf, int len, int32_t timeout) { 
	int ret = -1; 
	uint8_t *read_buf = NULL;
	DATETIME maxtime,  ssl_maxtime, current_time;

	/* maxtime is used in conjunction with the socket read timeout to quickly read the socket */
	maxtime	= twGetSystemMillisecondCount() + timeout;

	/* the ssl_read timeout is used if the ssl header is read but the socket timeout is reached		*/
	/* before the ssl encrypted data can be read													*/
	/* in this case, the read will be allowed to continue until the ssl_maxtime is reached			*/
	/* this will eliminate the possibility of timing out a read but returining a success since the	*/
	/* initial header was read																		*/
	/* twcfg.ssl_read_timeout is added to maxtime to ensure that ssl_maxtime>=maxtime */
	ssl_maxtime = maxtime + twcfg.ssl_read_timeout;

    if(!ssl || !buf) return -1;
	/* Anything left from last read? */
	if (ssl->leftover_cnt && ssl->leftover_ptr) {
		uint16_t bytes = ssl->leftover_cnt > len ? len : ssl->leftover_cnt;
		memcpy(buf, ssl->leftover_ptr, bytes);
		if (ssl->leftover_cnt > len) {
			ssl->leftover_cnt -= len;
			ssl->leftover_ptr += bytes;
		} else {
			ssl->leftover_cnt = 0;
			ssl->leftover_ptr = ssl->leftover_buf;
		}
		TW_AXTLS_LOG("AXTLS:SSL_READ - Reading from leftover data.  Asked for %d, Returned %d", len, bytes);
		return bytes;
	}
	
	twMutex_Lock(ssl->read_write_mutex);
	TW_AXTLS_LOG("TW_SSL_READ: About to read");
	/* Loop until we are unblocked or timeout */
	while ((ret = ssl_read(ssl->ssl, &read_buf)) == SSL_OK) {
		/* if the socket timeout has been reached... */
		current_time = twGetSystemMillisecondCount();
		if (!twTimeGreaterThan(maxtime, current_time)) {
			TW_AXTLS_LOG("TW_SSL_READ: socket timeout");
			/* If the SSL_NEED_RECORD flag is cleared and the ssl_maxtime has not been reached, allow the read to	*/
			/* continue until the ssl encrypted data is read or the ssl_maxtime is reached						*/
			/* This will prevent the TW_SSL_READ from returning a success (because the header was read) when there	*/
			/* should have really been a timeout (because the socket timeout was reached)							*/
			/* This will also allow TW_SSL_READ to complete a read if there is actual data on the socket that came in	*/
			/* right at the end of the frame timeout																	*/
			if (!TW_AXTLS_IS_SET_SSL_FLAG && twTimeGreaterThan(ssl_maxtime, current_time)) {
				TW_AXTLS_LOG("TW_SSL_READ: header waitfor++");
				/* allow the read to continue for at least one more socket_timeout before checking the ssl_maxtime timeout */
				maxtime = twGetSystemMillisecondCount() + timeout;
			} else {
				/* debugging if/else statements
				/* if (!twTimeGreaterThan(header_maxtime, twGetSystemMillisecondCount())) {
					TW_AXTLS_LOG("TW_SSL_READ: header timeout");	
				} else {
					TW_AXTLS_LOG("TW_SSL_READ: no flag change");
				} */
				break;
			}
		} 
	}
	TW_AXTLS_LOG("TW_SSL_READ: read ret: %d", ret);
	twMutex_Unlock(ssl->read_write_mutex);

	if (ret > SSL_OK) {
		uint16_t bytes = ret > len ? len : ret;
		memcpy(buf, read_buf, bytes);
		ssl->leftover_cnt = ret > len ? ret - len : 0;
		TW_AXTLS_LOG("AXTLS:SSL_READ - Asked for %d bytes, Read %d bytes", len, ret);
		if (ssl->leftover_cnt) {
			if (ssl->leftover_cnt > ssl->max_leftover) {
				if (ssl->leftover_buf) TW_FREE(ssl->leftover_buf);
				TW_AXTLS_LOG("AXTLS:SSL_READ - Allocating %d bytes for leftover buffer", ssl->leftover_cnt);
				ssl->leftover_buf = (uint8_t *)TW_CALLOC(ssl->leftover_cnt,1);
				if (!ssl->leftover_buf) {
					TW_AXTLS_LOG("AXTLS - PANIC! Unable to allocate memory");
					return -1;
				}
				ssl->leftover_ptr = ssl->leftover_buf;
				ssl->max_leftover = ssl->leftover_cnt;
			}
			memcpy(ssl->leftover_buf, read_buf + bytes, ssl->leftover_cnt);
			TW_AXTLS_LOG("AXTLS:SSL_READ - Copied %d bytes to leftover buffer", ssl->leftover_cnt);
		}
		return bytes;
	} else if (ret == SSL_OK) {
		/* the WaitFor timed out, meaning we have no more data from the server */
		TW_AXTLS_LOG("AXTLS:SSL_READ - WaitFor timeout while already begun reading TLS packet");

		if (ssl->ssl->bm_all_data && strlen((char *)ssl->ssl->bm_all_data)){
			TW_AXTLS_LOG(TW_ERROR, "AXTLS:SSL_READ: SSL_READ - WaitFor timeout while already begun reading TLS packet. SSL buffer (ssl->bm_all_data) contains data. .");
		}

		return TW_READ_TIMEOUT;
	} else return ret; 
}

static INLINE int TW_SSL_WRITE(TW_SSL *ssl, char *out_data, int out_len) {
	int bytes = 0;
	twMutex_Lock(ssl->read_write_mutex);
	bytes = ssl_write(ssl->ssl, (uint8_t *)out_data, out_len);
	twMutex_Unlock(ssl->read_write_mutex);
	return bytes;
}

/**
 * \brief Wait for a TLS/SSL client to initiate a TLS/SSL handshake.
 *
 * \param[in]     s         The TLS/SSL connection to accept.
 *
 * \return 0 on success, -1 on failure.
*/
static int TW_SSL_ACCEPT(TW_SSL * s) {
	if (!s || !s->ssl) return -1;
    while (ssl_read(s->ssl, NULL) == SSL_OK) {
        if (s->ssl->next_state == HS_CLIENT_HELLO)
            return 0;   /* we're done */
    }
    return -1;
}

/**
 * \brief Validates the certificate of a TLS/SSL connection.
 *
 * \param[in]     s             The TSL/SSL connection to validate the certificate of.
 * \param[in]     selfSignedOk  If #TRUE, accept self-signed certificates, if
 *                              #FALSE, don't.
 *
 * \return 0 on success, -1 on failure.
*/
static INLINE int TW_VALIDATE_CERT(TW_SSL * ssl, char selfSignedOk) {
	int res = 1;
	if (!ssl || !ssl->ssl) return 1;
	res = ssl_verify_cert ( ssl->ssl ); 
	if (res) res = res - SSL_X509_OFFSET;
	if (res == SSL_OK) return 0;
	if( res != SSL_OK && (selfSignedOk && res == X509_VFY_ERROR_SELF_SIGNED)) return 0;
	return 1;
}

/**
 * \brief Gets a X509 certificate field of a TLS/SSL connection.
 *
 * \param[in]     s         The TSL/SSL connection to get the 509 certificate
 *                          field of.
 * \param[in]     field     The X509 certificate field to get.  May be
 *                          #SSL_X509_CERT_COMMON_NAME,
 *                          #SSL_X509_CERT_ORGANIZATION,
 *                          #SSL_X509_CERT_ORGANIZATIONAL_NAME,
 *                          #SSL_X509_CA_CERT_COMMON_NAME,
 *                          #SSL_X509_CA_CERT_ORGANIZATION, or
 *                          #SSL_X509_CA_CERT_ORGANIZATIONAL_NAME.
 *
 * \return A string containing the data from the specified \p field.  Returns
 * NULL if an error was encountered.
 *
 * \note Calling function will gain ownership of the returned string and is
 * responsible for freeing it..
*/
static INLINE char * TW_GET_X509_FIELD(TW_SSL * ssl, char field) {
	const char * tmp = NULL;
	if (!ssl || !ssl->ssl) return NULL;
	switch (field) {
	case TW_SUBJECT_CN:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CERT_COMMON_NAME);
		break;
	case TW_SUBJECT_O:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CERT_ORGANIZATION);
		break;
	case TW_SUBJECT_OU:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CERT_ORGANIZATIONAL_NAME);
		break;
	case TW_ISSUER_CN:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CA_CERT_COMMON_NAME);
		break;
	case TW_ISSUER_O:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CA_CERT_ORGANIZATION);
		break;
	case TW_ISSUER_OU:
		tmp = ssl_get_cert_dn(ssl->ssl, SSL_X509_CA_CERT_ORGANIZATIONAL_NAME);
		break;
	default:
		tmp = NULL;
	}
	return duplicateString(tmp);
}

#ifdef __cplusplus
}
#endif

#endif /* AXTLS_H  */
