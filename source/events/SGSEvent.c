#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../thirdparty/cJSON.h"
#include "SGSEvent.h"

#include <curl/curl.h>

/*
 * 
 * Below is the functions needed by the sgsSendEmail
 *
*/ 

const char mailBodyHead[] = "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n";

static const char *sample_payload[] = {
  "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n",
  "To: " "ti4605246@gmail.com" "\r\n",
  "From: " "ti4605246@gmail.com" "\r\n",
  "Cc: " "t4605246@gmail.com" "\r\n",
  "Message-ID: <3>\r\n",/*ID must be different everytime*/
  "Subject: 22222\r\n",
  "\r\n", /* empty line to divide headers from body, see RFC5322 */ 
  "Eat this zzz\r\n",
  "\r\n",
  "***This is an automatically generated email, please do not reply***\r\n",
  "\r\n",
  NULL
};

char payload_text[12][2048];

struct upload_status 
{
  int lines_read;
};
 
static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp)
{
  struct upload_status *upload_ctx = (struct upload_status *)userp;
  const char *data;
 
  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }
 
  data = payload_text[upload_ctx->lines_read];
 
  if(data) {
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    upload_ctx->lines_read++;
 
    return len;
  }
 
  return 0;
}

/*
 * 
 * Above is the functions needed by the sgsSendEmail
 *
*/ 

void sgsSendEmail(char *message)
{

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx;
    
    //Read in mailing list

    FILE *toList = NULL;
    FILE *fromList = NULL;
    FILE *ccList = NULL;
    FILE *Host = NULL;

    char buf[512];
    
    char from[512];
    char to[512];
    char cc[512];

    int i = 0, ret = -1, count = 100;
    pid_t pid;

    int out; //redirect standard output


    pid = fork(); //Create a child to do mailing for us 

    if(pid == -1)
    {

        printf("sgsSendEmail failed [fork() return -1]\n");
        return ;

    }
    else if(pid > 0)
    {

        /*
        count = 100;
        do
        {

            ret = waitpid(pid,NULL,WNOHANG);
            usleep(50000);
            count--;

        }while(ret == 0 && count > 0);

        if(ret <= 0)
        {

            printf("sendmail child hanged, ret return %d\n",ret);

        }
        */

        return; // Parent back to work

    }

    //If everything is ok, the following lines are only for child

    /* ****Start mailing procedure**** */

    upload_ctx.lines_read = 0;

    for(i = 0 ; i < 12 ; i ++)
    {

        memset(payload_text[i],'\0',sizeof(payload_text[i]));

    }

    //Fill in the data payload

    //Date (Seems not related to anything)

    strncpy(payload_text[0],mailBodyHead,sizeof(payload_text) - 1);

    //"To: [To]\r\n"

    toList = fopen("./mail/TO","r");

    memset(to,'\0',sizeof(to));
    fscanf(toList,"%s",to);

    fclose(toList);

    snprintf(payload_text[1],512,"To: %s\r\n",to);

    //"From: [From]\r\n"

    fromList = fopen("./mail/FROM","r");

    memset(from,'\0',sizeof(from));
    fscanf(fromList,"%s",from);

    fclose(fromList);

    snprintf(payload_text[2],512,"From: %s\r\n",from);

    //"CC: [CC]\r\n"

    ccList = fopen("./mail/CC","r");

    memset(cc,'\0',sizeof(cc));
    fscanf(ccList,"%s",cc);

    fclose(ccList);

    snprintf(payload_text[3],512,"CC: %s\r\n",cc);

    //"Message-ID:  <[ID]>\r\n"

    memset(buf,'\0',sizeof(buf));

    snprintf(buf,512,"%d",rand()/1000);

    snprintf(payload_text[4],512,"Message-ID: <%s>\r\n",buf);

    //"Subject:  [subject]\r\n"

    memset(buf,'\0',sizeof(buf));

    Host = popen("hostname", "r");

    fgets(buf, sizeof(buf) , Host);

    snprintf(payload_text[5],512,"Subject: %s\r\n",buf);

    //Empty line to divide head from content

    snprintf(payload_text[6],512,"\r\n");

    //Body

    snprintf(payload_text[7],512,"%s\r\n",message);

    //Empty line to end the content

    snprintf(payload_text[8],512,"\r\n");

    //ending message

    snprintf(payload_text[9],512,"***This is an automatically generated email, please do not reply***\r\n");

    //Empty line to end the content

    snprintf(payload_text[10],512,"\r\n");

    //11 Stay empty

    //payload_text[11]

    curl = curl_easy_init();
    if(curl) 
    {

        /* Set username and password */ 
        curl_easy_setopt(curl, CURLOPT_USERNAME, "ti4605246@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "ti4692690");
    
        /* This is the URL for your mailserver. Note the use of port 587 here,
        * instead of the normal SMTP port (25). Port 587 is commonly used for
        * secure mail submission (see RFC4403), but you should use whatever
        * matches your server configuration. */ 
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
    
        /* In this example, we'll start with a plain text connection, and upgrade
        * to Transport Layer Security (TLS) using the STARTTLS command. Be careful
        * of using CURLUSESSL_TRY here, because if TLS upgrade fails, the transfer
        * will continue anyway - see the security discussion in the libcurl
        * tutorial for more details. */ 
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    
        /* If your server doesn't have a valid certificate, then you can disable
        * part of the Transport Layer Security protection by setting the
        * CURLOPT_SSL_VERIFYPEER and CURLOPT_SSL_VERIFYHOST options to 0 (false).
        *   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        *   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        * That is, in general, a bad idea. It is still better than sending your
        * authentication details in plain text though.  Instead, you should get
        * the issuer certificate (or the host certificate if the certificate is
        * self-signed) and add it to the set of certificates that are known to
        * libcurl using CURLOPT_CAINFO and/or CURLOPT_CAPATH. See docs/SSLCERTS
        * for more information. */ 
        //curl_easy_setopt(curl, CURLOPT_CAINFO, "/home/kelier-nb/key.pem");
    
        /* Note that this option isn't strictly required, omitting it will result
        * in libcurl sending the MAIL FROM command with empty sender data. All
        * autoresponses should have an empty reverse-path, and should be directed
        * to the address in the reverse-path which triggered them. Otherwise,
        * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
        * details.
        */ 
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, to);
    
        /* Add two recipients, in this particular case they correspond to the
        * To: and Cc: addressees in the header, but they could be any kind of
        * recipient. */ 
        recipients = curl_slist_append(recipients, from);
        recipients = curl_slist_append(recipients, cc);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    
        /* We're using a callback function to specify the payload (the headers and
        * body of the message). You could just use the CURLOPT_READDATA option to
        * specify a FILE pointer to read from. */ 
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    
        /* Since the traffic will be encrypted, it is very useful to turn on debug
        * information within libcurl to see what is happening during the transfer.
        */ 
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    
        /* Send the message */ 
        res = curl_easy_perform(curl);
    
        /* Check for errors */ 
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    
        /* Free the list of recipients */ 
        curl_slist_free_all(recipients);
    
        /* Always cleanup */ 
        curl_easy_cleanup(curl);
    }
    printf(NONE);
    exit(0);

}

void sgsShowErrMsg()
{

    printf("Errmsg: %s\n",sgsErrMsg);
    return;

}

char* sgsGetErrMsg()
{

    return sgsErrMsg;

}

void sgsSetErrNum(unsigned int errNum)
{

    sgsErrNum = errNum;
    return;

}

void sgsSetErrMsg(char *message)
{

    strncpy(sgsErrMsg, message, sizeof(sgsErrMsg));
    return;

}