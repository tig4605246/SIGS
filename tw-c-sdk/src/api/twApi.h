/***************************************
 *  Copyright (C) 2015 ThingWorx Inc.  *
 ***************************************/

/**
 * \file twApi.h
 * \brief Portable ThingWorx C SDK API layer
 *
 * Contains structure type definitions and function prototypes for the
 * ThingWorx API.
*/

#ifndef TW_API_H
#define TW_API_H

#include "twOSPort.h"
#include "twDefinitions.h"
#include "twDefaultSettings.h"
#include "twLogger.h"
#include "twBaseTypes.h"
#include "twMessaging.h"
#include "twInfoTable.h"
#include "twTasker.h"
#include "twConnectionInfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define propertyList twList /* A propertyList is a twList */

/**
 * \name Generic callback data structure
 *
 * \note Internal structure to handle callback information.  
 * There should be no need to manipulate this structure directly.
*/
typedef struct callbackInfo {
	enum entityTypeEnum entityType;
	char * entityName;
	enum characteristicEnum characteristicType;
	char * charateristicName;
	void * charateristicDefinition;
	void * cb;
	void * userdata;
} callbackInfo;

/**
 * \name Callback Function Signatures
*/

/**
 * \brief Signature of a callback function that is registered to be called when
 * a specific property request is received from the ThingWorx server.
 *
 * \param[in]     entityName    Name of the entity (Thing, Resource, etc.) this
 *                              request is for.  
 * \param[in]     propertyName  Name of the property being requested.  If NULL,
 *                              return all properties.  
 * \param[in,out] value         A pointer to a pointer to a ::twInfoTable
 *                              containing the value of the property.
 * \param[in]     isWrite       #TRUE if this request is a write, #FALSE if
 *                              this request is a read.
 * \param[in]     userdata      An opaque pointer that is passed in when the
 *                              callback is registered.
 *
 * \return #TWX_SUCCESS if the request completes successfully, an appropriate
 * error code if not (see ::msgCodeEnum).
 *
 * \note \p entityName is guaranteed to not be NULL.
 * \note \p value is guaranteed to not be NULL.
*/
typedef enum msgCodeEnum (*property_cb) (const char * entityName, const char * propertyName,  twInfoTable ** value, char isWrite, void * userdata);

/**
 * \brief Signature of a callback function that is registered to be called when
 * a specific service request is received from the ThingWorx server.
 *
 * \param[in]     entityName    Name of the entity (Thing, Resource, etc.) this
 *                              request is for.
 * \param[in]     serviceName   Name of the service being requested.
 * \param[in]     params        A pointer to a ::twInfoTable containing all
 *                              input parameters for the service.
 * \param[out]    content       A pointer to a pointer to a ::twInfoTable.
 * \param[in]     userdata      An opaque pointer that is passed in when the
 *                              callback is registered.
 *
 * \return #TWX_SUCCESS if the request completes successfully, an appropriate
 * error code if not (see ::msgCodeEnum).
 *
 * \note \p entityName is guaranteed to not be NULL.
 * \note \p content is guaranteed to not be NULL, \p *content is not.
 * \note The function should create a new instance of a ::twInfoTable on the
 * heap and return it via \p content.
 * \note Calling function will retain ownership of the \p content pointer and
 * is responsible for freeing it.
*/
typedef enum msgCodeEnum (*service_cb) (const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata);

/**
 * \brief Signature of a callback function that is registered to be called for
 * unhandled requests.
 *
 * \param[in]     msg           A ::twMessage structure.
 *
 * \return A pointer to the response message.
 *
 * \note Registered via twApi_RegisterDefaultRequestHandler().
 * \warning The user must keep the message ID the same as the request.
*/
typedef twMessage * (*genericRequest_cb)(twMessage * msg);

/**
 * \brief Signature of a callback function that is registered to be called when
 * a bind or unbind completes.
 *
 * \param[in]     entityName    Name of the entity (Thing, Resource, etc.) this
 *                              request is for.
 * \param[in]     isBound       #TRUE if the entity was bound, #FALSE if the
 *                              entity was unbound.
 * \param[in]     userdata      An opaque pointer that is passed in when the
 *                              callback is registered.
 *
 * \return Nothing.
 *
 * \note This callback is an indication that the entity that was bound can
 * (bind) or can no longer (unbind) be used by the application.
 * \note \p entityName is guaranteed to not be NULL.
*/
typedef void (*bindEvent_cb)(char * entityName, char isBound, void * userdata);

/**
 * \brief Signature of a callback function that is registered to be called when
 * authentication to the ThingWorx server completes.
 *
 * \param[in]     credentialType    The type of credential that was used to
 *                                  authenticate.
 * \param[in]     credentialValue   The value for the credential that was
 *                                  passed to the ThingWorx server.
 * \param[in]     userdata          An opaque pointer that is passed in when
 *                                  the callback is registered. 
 *
 * \return Nothing.
 *
 * \note This callback is an indication that the connection to the ThingWorx
 * server is fully up and accessible.
 * \note \p credentialType is guaranteed to not be NULL.
*/
typedef void (*authEvent_cb)(char * credentialType, char * credentialValue, void * userdata);

/**
 * \name API Structure Definition
*/

/**
 * \brief ThingWorx API structure definition.
 *
 * \note A singleton instance of this structure is automatically created when
 * twApi_initialize() is called.  There should be no need to manipulate this
 * structure directly.
*/
typedef struct twApi {
	twMessageHandler * mh;                   /**< Pointer to the ThingWorx message handler. **/
	twList * callbackList;                   /**< Pointer to a ::twList of callbacks. **/
	twList * boundList;                      /**< Pointer to a ::twList of bound entities. **/
	twList * bindEventCallbackList;          /**< Pointer to a ::twList of bind event callbacks. **/
	genericRequest_cb defaultRequestHandler; /**< The default request handler. **/
	char autoreconnect;                      /**< #TRUE if automatic reconnection to ThingWorx server is enabled. **/
	int8_t manuallyDisconnected;             /**< #TRUE if the API was manually disconnected from the ThingWorx server. **/
	char isAuthenticated;                    /**< Authentication status flag. **/
	uint8_t duty_cycle;                      /**< The duty cycle of the connection in percent (1-100). **/
	uint32_t duty_cycle_period;              /**< The connection period (in milliseconds).  Value of 0 indicates AlwaysOn and overrides duty_cycle. **/
	TW_MUTEX mtx;                            /**< ThingWorx mutex. **/
	char offlineMsgEnabled;                  /**< Offline message enabled flag. **/
	twList * offlineMsgList;                 /**< Pointer to a ::twList of offline messages. **/
	uint32_t offlineMsgSize;                 /**< The size of the offline message(s). **/
	char * offlineMsgFile;                   /**< The offline message filename. **/
	char * subscribedPropsFile;              /**< Subscribed properties file. **/
	uint32_t subscribedPropsSize;            /**< Subscribed properties size. **/
	uint32_t ping_rate;                      /**< The websockets Ping/Pong interval. **/
	char handle_pongs;                       /**< #TRUE if Pongs are currently handled in the API (see twApi_RegisterPongCallback()). **/
	uint32_t connect_timeout;                /**< How long to wait for the websocket to be established (in milliseconds). **/
	int16_t connect_retries;                 /**< The number of times to attempt to reconnect if the connection fails. **/
	char connectionInProgress;               /**< Connection in progress flag. **/
	char firstConnectionComplete;            /**< First connection completion flag. **/
	twConnectionInfo * connectionInfo;       /**< ::twConnectionInfo associated with the API. **/
} twApi;


/**
 * \name Lifecycle Functions
*/

/**
 * \brief Creates the ::twApi singleton and any dependent structures.
 *
 * \param[in]     host              The hostname of the ThingWorx server to
 *                                  connect to. 
 * \param[in]     port              The TCP port number to be used by the
 *                                  ThingWorx server.
 * \param[in]     resource          ThingWorx server resource (should always be
 *                                  /ThingWorx/WS unless changed by the server.
 * \param[in]     app_key           The application key defined on the
 *                                  ThingWorx server that this entity should
 *                                  use as an authentication token.
 * \param[in]     gatewayName       An optional name to register with if the
 *                                  application is acting as a gateway for
 *                                  multiple Things.
 * \param[in]     messageChunkSize  The maximum chunk of a websock message
 *                                  (should match the ThingWorx server).
 *                                  Default is 8192 and should not be exceeded.
 * \param[in]     frameSize         The maximum size of a websocket frame.
 *                                  Ordinarily matches \p messageChunkSize.
 * \param[in]     autoreconnect     #TRUE enables automatic reconnection to
 *                                  ThingWorx server if a connection is lost.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_Initialize(char * host, uint16_t port, char * resource, char * app_key, char * gatewayName,
				     uint32_t messageChunkSize, uint16_t frameSize, char autoreconnect);

/**
 * \brief Shuts down the websocket and frees all memory associated with the
 * ::twApi structure and all its owned substructures.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_Delete();

/**
 * \brief Returns a constant pointer to the version string of the API.
 *
 * \return The current version of the API as a constant string.
*/
char * twApi_GetVersion();

/**
 * \name Connection Functions
*/

/**
 * \brief Establishes the websocket connection, performs authentication and
 * binds any registered Things.
 *
 * \param[in]     timeout           How long to wait for the websocket to be
 *                                  established (in milliseconds).
 * \param[in]     retries           The number of times to attempt to reconnect
 *                                  if the connection fails.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_Connect(uint32_t timeout, int32_t retries);

/**
 * \brief Unbinds any bound entities and disconnects from the ThingWorx server.
 *
 * \param[in]     reason            The reason for disconnecting.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_Disconnect(char * reason);

/**
 * \brief Changes the duty cycle (twApi#duty_cycle) and period
 * (twApi#duty_cycle_period) of the connection.
 *
 * \param[in]     duty_cycle        The duty cycle of the connection in percent
 *                                  (1-100).  Values over 100 will be set to 100.
 * \param[in]     period            The connection period (in milliseconds).
 *                                  Value of 0 indicates AlwaysOn and overrides duty_cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_SetDutyCycle(uint8_t duty_cycle, uint32_t period);

/**
 * \brief Passthru to notify the TLS library to accept self-signed
 * certificates.
 *
 * \return Nothing.
*/
void twApi_SetSelfSignedOk();

/**
 * \brief Passthru to enable FIPS mode for TLS providers that support it.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_EnableFipsMode();

/**
 * \brief Passthru to notify the TLS library to not validate the ThingWorx
 * server certificate.
 *
 * \return Nothing.
 *
 * \warning Disabling certificate validation may induce a security risk.
*/
void twApi_DisableCertValidation();

/**
 * \brief Passthru to notify the TLS library to disable encryption.
 *
 * \return Nothing.
 *
 * \warning Disabling encryption may induce a security risk<.
*/
void twApi_DisableEncryption();

/**
 * \brief Defines which fields of an X509 certificate will be validated.
 *
 * \param[in]     subject_cn        The common name of the subject in the
 *                                  certificate. 
 * \param[in]     subject_o         The organization of the subject in the
 *                                  certificate.
 * \param[in]     subject_ou        The organizational unit of the subject in
 *                                  the certificate.
 * \param[in]     issuer_cn         The common name of the issuer in the
 *                                  certificate.
 * \param[in]     issuer_o          The organization of the issuer in the
 *                                  certificate.
 * \param[in]     issuer_ou         The organizational unit of the issuer in
 *                                  the certificate.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note NULL values will not be checked against the deceived certificate,
 * non-NULL values will be.
*/
int twApi_SetX509Fields(char * subject_cn, char * subject_o, char * subject_ou,
					    char * issuer_cn, char * issuer_o, char * issuer_ou);

/**
 * \brief Loads the local PEM or DER formatted certificate file used to
 * validate the ThingWorx server.
 *
 * \param[in]     issuer_ou         The organizational unit of the issuer in
 *                                  the certificate.
 * \param[in]     file              The full path to the file containing the
 *                                  certificate.
 * \param[in]     type              Definition is dependent on the underlying
 *                                  TLS library (can be set to 0 for AxTLS).
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int	twApi_LoadCACert(const char *file, int type);

/**
 * \brief Loads the local PEM or DER formatted certificate file used to
 * validate the client to the ThingWorx server.
 *
 * \param[in]     file              The full path to the file containing the
 *                                  certificate.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int	twApi_LoadClientCert(char *file);

/**
 * \brief Sets the passphrase key of the local PEM or DER formatted certificate
 * file used to validate the client to the ThingWorx server.
 *
 * \param[in]     file              The full path to the file containing the
 *                                  certificate.
 * \param[in]     passphrase        The passphrase used to open the file. 
 * \param[in]     type              Definition is dependent on the underlying
 *                                  TLS library.  Can be set to 0 for AxTLS.
 *
 * \return #TW_OK if successful, positive integral on error code (see twErrors.h) if an error was encountered.
*/
int	twApi_SetClientKey(const char *file, char * passphrase, int type);

/**
 * \brief Sets the websockets ping/pong interval (see twApi#ping_rate).
 *
 * \param[in]     rate              The new ping rate (in milliseconds).
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The twApi#ping_rate must align with twApi#connect_timeout.
*/
int twApi_SetPingRate(uint32_t rate);

/**
 * \brief Sets the amount of time (in milliseconds) the websocket waits while
 * attempting a connection (see twApi#connect_timeout).
 *
 * \param[in]     timeout           The new timeout (in milliseconds).
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The twApi#ping_rate must align with twApi#connect_timeout.
*/
int twApi_SetConnectTimeout(uint32_t timeout);

/**
 * \brief Sets the number of times to attempt to reconnect if the connection
 * fails (see twApi#connect_retries).
 *
 * \param[in]     retries           The number of times to attempt to reconnect
 *                                  if the connection fails.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_SetConnectRetries(signed char retries);


/**
 * \brief Sets the Gateway Name that will be bound with the platform
 *
 * \param[in]     input_name        The Gateway name as configured on the edge
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_SetGatewayName(const char* input_name);

/**
 * \brief Sets the Gateway Type that will be bound with the platform
 *
 * \param[in]     input_type        The Gateway name as configured on the edge
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_SetGatewayType(const char* input_type);
/**
 * \brief Clears the proxy information of the socket to be used when making a connection.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_ClearProxyInfo();

/**
 * \brief Sets the proxy information to be used when making a connection.
 *
 * \param[in]     retries           The number of times to attempt to reconnect
 *                                  if the connection fails.
 * \param[in]     proxyHost         The host name of the proxy.
 * \param[in]     proxyPort         The port used by the proxy.
 * \param[in]     proxyUser         The username to supply to the proxy (can be
 *                                  NULL if the proxy fails to authenticate).
 * \param[in]     proxyPass         The password to supply to the proxy (can be
 *                                  NULL if the proxy fails to authenticate).
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_SetProxyInfo(char * proxyHost, uint16_t proxyPort, char * proxyUser, char * proxyPass);

/**
 * \brief Creates a copy of the current connection info of the ThingWorx
 * server.
 *
 * \return Pointer to a copy of the API connection info.
 *
 * \note The API retains ownership of returned pointer and the calling function
 * must <b>not</b> delete it.
*/
twConnectionInfo * twApi_GetConnectionInfo();

/**
 * \brief Sets the offline message store directory.
 *  
 * \param[in]     dir               The full path to the directory where the
 *                                  offline message store file should be
 *                                  created.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \warning If the current offline msg store file is not empty an error will be
 * returned and the directory will <b>not<\b> be changed.
*/
int twApi_SetOfflineMsgStoreDir(const char *dir);

/**
 * \name Connection Status Functions
*/

/**
 * \brief Registers a function to be called when the web socket connects (see
 * callback function signature eventcb()).
 *  
 * \param[in]     cb                The function to be called.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterConnectCallback(eventcb cb);

/**
 * \brief Registers a function to be called when the websocket disconnects (see
 * callback function signature eventcb()).
 *  
 * \param[in]     cb                The function to be called.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterCloseCallback(eventcb cb);

/**
 * \brief Checks to see if the API websocket is connected.
 *
 * \return #TRUE if connected, #FALSE if not connected.
*/
char twApi_isConnected();

/**
 * \brief Checks to see if the API is in the process of connecting to the
 * websocket.
 *
 * \return #TRUE if connecting, #FALSE if not connecting.
*/
char twApi_ConnectionInProgress();

/**
 * \brief Kills any long running connection attempt.
 *  
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_StopConnectionAttempt();

/**
 * \name Binding Functions
*/

/**
 * \brief Bind an entity to this connection with the ThingWorx server.
 *  
 * \param[in]     entityName        The name of the entity to bind with the
 *                                  ThingWorx server.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note If there is currently an active connection a bind message is sent.  If
 * not, the bind message will be sent on the next connection.
*/
int twApi_BindThing(char * entityName);

/**
 * \brief Unbind an entity from this connection with the ThingWorx server.
 *  
 * \param[in]     entityName        The name of the entity to unbind from the
 *                                  ThingWorx server.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note If there is currently an active connection an unbind message is sent.
 * If not, the unbind message will be sent on the next connection.
*/
int twApi_UnbindThing(char * entityName);

/**
 * \brief Registers a function to be called when an entity is bound or unbound
 * (see callback function signature bindEvent_cb()).
 *  
 * \param[in]     entityName        Callbacks are filtered to the specified
 *                                  entity.  A NULL value receives all callbacks.
 * \param[in]     cb                The function to be called.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterBindEventCallback(char * entityName, bindEvent_cb cb, void * userdata);

/**
 * \brief Unregisters a callback registered via
 * twApi_RegisterBindEventCallback() (see callback function signature
 * bindEvent_cb()).
 *  
 * \param[in]     entityName        Callbacks are filtered to the specified
 *                                  entity.  A NULL value receives all callbacks.
 * \param[in]     cb                The function to be unregistered.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UnregisterBindEventCallback(char * entityName, bindEvent_cb cb, void * userdata);

/**
 * \brief Registers a function to be called when the connection to the
 * ThingWorx server is fully authenticated (see callback function signature
 * authEvent_cb()).
 *  
 * \param[in]     cb                The function to be called.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterOnAuthenticatedCallback(authEvent_cb cb, void * userdata);

/**
 * \brief Unregisters a callback registered via
 * twApi_RegisterOnAuthenticatedCallback() (see callback function signature
 * authEvent_cb()).
 *  
 * \param[in]     cb                The function to be unregistered.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UnregisterOnAuthenticatedCallback(authEvent_cb cb, void * userdata);

/**
 * \brief Checks to see if an entity is bound through the API.
 *
 * \param[in]     entityName        The name of the entity to check.
 *
 * \return #TRUE if bound, #FALSE if not bound.
*/
char twApi_IsEntityBound(char * entityName);

/**
 * \name Operational Functions
*/

/**
 * \brief Executes all functions required for proper operation of the API.  This
 * includes the connection receive loop, duty cycle control, stale message
 * cleanup, ping/pong, etc.
 *
 * \param[in]     now               The current timestamp.
 * \param[in]     params            Required by the tasker function signature
 *                                  but currently unused.
 *
 * \return Nothing.
 *
 * \note This function should be called at a regular rate every 5 milliseconds
 * or so depending on your tolerance for system latency.  This function is
 * automatically called by the tasker.
*/
void twApi_TaskerFunction(DATETIME now, void * params);

/**
 * \name Property and Service Callback Registration Functions
*/

/**
 * \brief Registers a property and callback.  This property will be reported
 * back to the ThingWorx server when it is browsing (see callback function
 * signature property_cb()).
 *
 * \param[in]     entityType            The type of entity that the property
 *                                      belongs to (see ::entityTypeEnum).
 * \param[in]     entityName            The name of the entity that the
 *                                      property belongs to.
 * \param[in]     propertyName          The name of the property to be
 *                                      registered.
 * \param[in]     propertyType          The ::BaseType of the property.
 * \param[in]     propertyPushType      The push type of the property.  Can be
 *                                      set to #NEVER, #ALWAYS, or #VALUE (on
 *                                      change).
 * \param[in]     propertyPushThreshold The amount the property has to change
 *                                      (if the type is #TW_NUMBER or
 *                                      #TW_INTEGER) before pushing the new
 *                                      value.
 * \param[in]     cb                    Pointer to the property callback
 *                                      function.
 * \param[in]     userdata              An opaque pointer that is passed into
 *                                      the callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, enum BaseType propertyType, 
						   char * propertyDescription, char * propertyPushType, double propertyPushThreshold, property_cb cb, void * userdata);

/**
 * \brief Adds an aspect to an already registered property.  
 *
 * \param[in]     entityName            The name of the entity that the
 *                                      property belongs to.
 * \param[in]     propertyName          The name of the property to add the aspect to.
 * \param[in]     aspectName            The name of the aspect.
 * \param[in]     aspectValue           The value of the aspect expressed as a 
 *                                      ::twPrimitive. The called function takes 
 *                                      ownership of the primitive and will delete it.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_AddAspectToProperty(char * entityName, char * propertyName,  
							  char * aspectName, twPrimitive * aspectValue);

/**
 * \brief Updates the metadata aspects of a property.  
 *
 * \param[in]     entityType            The type of the entity the property is associated with.
 * \param[in]     entityName            The name of the entity that the
 *                                      property belongs to.
 * \param[in]     propertyName          The name of the property to update.
 * \param[in]     propertyType          The BaseType of the property.  See BaseTypes definition in twDefinitions.h.  
                                        TW_UNKNOWN_TYPE keeps the current type.
 * \param[in]     propertyDescription   The description of the property.  NULL means keep the current description.
 * \param[in]     propertyPushType      The push type of the property.  Can be NEVER, ALWAYS, VALUE (on change).
 * \param[in]     propertyPushThreshold The amount the property has to change (if the type is TW_NUMBER or TW_INTEGER) 
                                        before pushing the new value.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UpdatePropertyMetaData(enum entityTypeEnum entityType, char * entityName, char * propertyName, enum BaseType propertyType, 
						   char * propertyDescription, char * propertyPushType, double propertyPushThreshold);

/**
 * \brief Registers a service and callback.  This service will be reported back
 * to the ThingWorx server when it is browsing (see callback function signature
 * property_cb()).
 *
 * \param[in]     entityType            The type of entity that the property
 *                                      belongs to (see ::entityTypeEnum).
 * \param[in]     entityName            The name of the entity that the service
 *                                      belongs to.
 * \param[in]     serviceName           The name of the service to be
 *                                      registered.
 * \param[in]     serviceDescription    A description of the service to be
 *                                      registered.
 * \param[in]     inputs                A ::twDataShape that describes the
 *                                      service input.
 * \param[in]     outputType            The ::BaseType of the service result.
 * \param[in]     outputDataShape       A ::twDataShape that described the
 *                                      service output if the output is a
 *                                      ::twInfoTable.
 * \param[in]     cb                    Pointer to the service callback
 *                                      function.
 * \param[in]     userdata              An opaque pointer that is passed into
 *                                      the callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterService(enum entityTypeEnum entityType, char * entityName, char * serviceName, char * serviceDescription,
						  twDataShape * inputs, enum BaseType outputType, twDataShape * outputDataShape, service_cb cb, void * userdata);

/**
 * \brief Adds an aspect to an already registered service.  
 *
 * \param[in]     entityName            The name of the entity that the
 *                                      property belongs to.
 * \param[in]     serviceName           The name of the service to add the aspect to.
 * \param[in]     aspectName            The name of the aspect.
 * \param[in]     aspectValue           The value of the aspect expressed as a 
 *                                      ::twPrimitive. The called function takes 
 *                                      ownership of the primitive and will delete it.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_AddAspectToService(char * entityName, char * serviceName,  
							  char * aspectName, twPrimitive * aspectValue);

/*
twApi_RegisterEvent - register an event.  This event will be reported back to the server when it is browsing. Note that Events do not have callbacks since
                      they not invokeable from the server to the edge.
Parameters:
	entityType - the type of entity that the property belongs to. Enum can be found in twDefinitions.h
	entityName - the name of the entity that the property belongs to.
	eventName - the name of the service.
	eventDescription - description of the service
	inputs - a datashape that describes the event parameters.  See twInfoTable for the twDataShape definition.
Return:
	int - 0 if successful, positive integral error code (see twErrors.h) if an was encountered
*/
int twApi_RegisterEvent(enum entityTypeEnum entityType, char * entityName, char * eventName, char * eventDescription, twDataShape * parameters);

/**
 * \brief Adds an aspect to an already registered event.  
 *
 * \param[in]     entityName            The name of the entity that the
 *                                      property belongs to.
 * \param[in]     eventName             The name of the event to add the aspect to.
 * \param[in]     aspectName            The name of the aspect.
 * \param[in]     aspectValue           The value of the aspect expressed as a 
 *                                      ::twPrimitive. The called function takes 
 *                                      ownership of the primitive and will delete it.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_AddAspectToService(char * entityName, char * eventName,  
							  char * aspectName, twPrimitive * aspectValue);

/**
 * \brief Removes a property from the callback list.  This service will no
 * longer be reported back to the ThingWorx server while browsing.
 *
 * \param[in]     entityName        The name of the entity that the
 *                                  property belongs to.
 * \param[in]     propertyName      The name of the property to be
 *                                  unregistered.
 * \param[in]     userdata          An opaque pointer that is passed into
 *                                  the callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UnregisterPropertyCallback(char * entityName, char * propertyName, void * userdata);

/**
 * \brief Removes a service from the callback list.  This service will no longer
 * be reported back to the ThingWorx server while browsing.
 *
 * \param[in]     entityName        The name of the entity that the service
 *                                  belongs to.
 * \param[in]     serviceName       The name of the service to be
 *                                  unregistered.
 * \param[in]     userdata          An opaque pointer that is passed into
 *                                  the callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UnregisterServiceCallback(char * entityName, char * serviceName, void * userdata);

/**
 * \brief Removes all property & service callbacks for an entity.
 *
 * \param[in]     entityName        The name of the entity.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_UnregisterThing(char * entityName);

/**
 * \brief Registers a service callback function that will get called for all
 * unhandled requests (see callback function signature genericRequest_cb()).
 *
 * \param[in]     cb                Pointer to the generic callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterDefaultRequestHandler(genericRequest_cb cb);

/**
 * \brief Register a property callback only (see callback function signature
 * property_cb()).
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the property
 *                                  belongs to.
 * \param[in]     propertyName      The name of the property to be registered.
 *                                  Value of "*" registers the callback for all
 *                                  property requests for the specified entity.
 * \param[in]     cb                Pointer to the generic callback function.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note This property will <b>not</b> be reported back to the ThingWorx server
 * when it is browsing.
*/
int twApi_RegisterPropertyCallback(enum entityTypeEnum entityType, char * entityName, char * propertyName, property_cb cb, void * userdata);

/**
 * \brief Register a service callback only (see callback function signature
 * service_cb()).
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the service
 *                                  belongs to.
 * \param[in]     serviceName       The name of the service to be registered.
 *                                  Value of "*" registers the callback for all
 *                                  service requests for the specified entity.
 * \param[in]     cb                Pointer to the generic callback function.
 * \param[in]     userdata          An opaque pointer that is passed into the
 *                                  callback function.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note This service will <b>not</b> be reported back to the ThingWorx server
 * when it is browsing.
*/
int twApi_RegisterServiceCallback(enum entityTypeEnum entityType, char * entityName, char * serviceName, service_cb cb, void * userdata);

/**
 * \name Server Property/Service/Event Accessor Functions
*/

/**
 * \brief Creates a list of properties.
 *
 * \param[in]     name              The name of the first property to add to
 *                                  the list.
 * \param[in]     value             A pointer to the primitive containing the
 *                                  first property value. 
 * \param[in]     timestamp         Timestamp of the first property (defaults
 *                                  to current time).
 *
 * \return Pointer to the created property list.  Returns NULL on error.
 *
 * \note The newly allocated list will gain ownership the \p value pointer.
*/
propertyList * twApi_CreatePropertyList(char * name, twPrimitive * value, DATETIME timestamp);

/**
 * \brief Frees all memory associated with a ::propertyList and all its owned
 * substructures.
 *
 * \param[in]     list              A pointer to the list to delete.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_DeletePropertyList(propertyList * list);

/**
 * \brief Adds a property to a property list.
 *
 * \param[in]     proplist          A pointer to the list to add the property
 *                                  to.
 * \param[in]     name              The name of the property to add to the
 *                                  list.
 * \param[in]     value             A pointer to the primitive containing the
 *                                  property type and value.
 * \param[in]     timestamp         Timestamp of the first property (defaults
 *                                  to current time).
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The newly allocated list will gain ownership the \p value pointer.
*/
int twApi_AddPropertyToList(propertyList * proplist, char * name, twPrimitive * value, DATETIME timestamp);

/**
 * \brief Gets the current value of a property from the ThingWorx server.
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the property
 *                                  belongs to.
 * \param[in]     propertyName      The name of the property to be read.
 * \param[out]    result            Pointer to a ::twPrimitive pointer.
 * \param[in]     timeout           Time (in milliseconds) to wait for a
 *                                  response from the ThingWorx server.  -1 uses
 *                                  #DEFAULT_MESSAGE_TIMEOUT.
 * \param[in]     forceConnect      #TRUE forces a reconnect and send if
 *                                  currently in the disconnected state of the
 *                                  duty cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The calling function will retain ownership of the \p result pointer
 * and is responsible for freeing it.
*/
int twApi_ReadProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, twPrimitive ** result, int32_t timeout, char forceConnect);

/**
 * \brief Writes a new value of the property to the ThingWorx server.
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the property
 *                                  belongs to.
 * \param[in]     propertyName      The name of the property to be written to.
 * \param[out]    value             Pointer to a ::twPrimitive pointer
 *                                  containing the new property value.
 * \param[in]     timeout           Time (in milliseconds) to wait for a
 *                                  response from the ThingWorx server.  -1 uses
 *                                  #DEFAULT_MESSAGE_TIMEOUT.
 * \param[in]     forceConnect      #TRUE forces a reconnect and send if
 *                                  currently in the disconnected state of the
 *                                  duty cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The calling function will retain ownership of the \p value pointer and
 * is responsible for freeing it.
*/
int twApi_WriteProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, twPrimitive * value, int32_t timeout, char forceConnect);

/**
 * \brief Writes a set of values of various properties (stored in a
 * propertyList) to the ThingWorx server.
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the properties
 *                                  belong to.
 * \param[in]     properties        A ::twList of ::twProperty pointers
 *                                  containing the values of the properties to
 *                                  write.
 * \param[in]     timeout           Time (in milliseconds) to wait for a
 *                                  response from the ThingWorx server.  -1 uses
 *                                  #DEFAULT_MESSAGE_TIMEOUT.
 * \param[in]     forceConnect      #TRUE forces a reconnect and send if
 *                                  currently in the disconnected state of the
 *                                  duty cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The calling function will retain ownership of the \p properties
 * pointer and is responsible for freeing it.
*/
int twApi_PushProperties(enum entityTypeEnum entityType, char * entityName, propertyList * properties, int32_t timeout, char forceConnect);

/**
 * \brief Invokes a service on the ThingWorx server.
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the service
 *                                  belongs to.
 * \param[in]     serviceName       The name of the service to be invoked.
 * \param[in]     params            A pointer to a ::twInfoTable containing the
 *                                  service parameters.
 * \param[out]    result            A pointer to a ::twInfoTable containing the
 *                                  service response.
 * \param[in]     timeout           Time (in milliseconds) to wait for a
 *                                  response from the ThingWorx server.  -1 uses
 *                                  #DEFAULT_MESSAGE_TIMEOUT.
 * \param[in]     forceConnect      #TRUE forces a reconnect and send if
 *                                  currently in the disconnected state of the
 *                                  duty cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The calling function will retain ownership of the \p result pointer
 * and is responsible for freeing it.
 * \note The calling function will retain ownership of the \p params pointer
 * and is responsible for freeing it.
*/
int twApi_InvokeService(enum entityTypeEnum entityType, char * entityName, char * serviceName, twInfoTable * params, twInfoTable ** result, int32_t timeout, char forceConnect);

/**
 * \brief Invokes an event on the ThingWorx server.
 *
 * \param[in]     entityType        The type of entity that the property
 *                                  belongs to (see ::entityTypeEnum).
 * \param[in]     entityName        The name of the entity that the event
 *                                  belongs to.
 * \param[in]     eventName         The name of the event to be triggered.
 * \param[in]     params            A pointer to a ::twInfoTable containing the
 *                                  event parameters.
 * \param[in]     timeout           Time (in milliseconds) to wait for a
 *                                  response from the ThingWorx server.  -1
 *                                  uses #DEFAULT_MESSAGE_TIMEOUT.
 * \param[in]     forceConnect      #TRUE forces a reconnect and send if
 *                                  currently in the disconnected state of the
 *                                  duty cycle.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The calling function will retain ownership of the \p params pointer
 * and is responsible for freeing it.
*/
int twApi_FireEvent(enum entityTypeEnum entityType, char * entityName, char * eventName, twInfoTable * params, int32_t timeout, char forceConnect);

/**
 * \brief Sets a new value, time and quality of the subscribed property. The subscribed property manager 
 * etermines if the new value will be pushed to the server based on the property's push setting and the new value.
 *
 * \param[in]     entityName        The name of the entity that the property
 *                                  belongs to.
 * \param[in]     propertyName      The name of the property to be written to.
 * \param[in]     value             Pointer to a ::twPrimitive pointer
 *                                  containing the new property value.
 * \param[in]     timestamp         The timestamp of the reading to send to the server
 *                                  Defaults to the current time.
 * \param[in]     quality           The quality of the reading.  NULL will default to "GOOD".
 * \param[in]     fold              (Boolean) enable property folding where only the most recent 
 *                                  value will be sent to the server on a push.  Default is all values are sent.
 * \param[in]     pushUpdate        (Boolean) All queue properties will be pushed to the server.  
 *                                  Push will only happen if websocket is connected and authenticated.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The called function will retain ownership of the \p value pointer. Calling function must NOT delete it.
*/
int twApi_SetSubscribedPropertyVTQ(char * entityName, char * propertyName, twPrimitive * value,  DATETIME timestamp, char * quality, char fold, char pushUpdate);

/**
 * \brief Sets a new value for the subscribed property. The subscribed property manager 
 * determines if the new value will be pushed to the server based on the property's push setting and the new value.
 *
 * \param[in]     entityName        The name of the entity that the property
 *                                  belongs to.
 * \param[in]     propertyName      The name of the property to be written to.
 * \param[in]     quality           The quality of the reading.  NULL will default to "GOOD".
 * \param[in]     fold              (Boolean) enable property folding where only the most recent 
 *                                  value will be sent to the server on a push.  Default is all values are sent.
 * \param[in]     pushUpdate        (Boolean) All queue properties will be pushed to the server.  
 *                                  Push will only happen if websocket is connected and authenticated.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note The called function will retain ownership of the \p value pointer. Calling function must NOT delete it.
*/
int twApi_SetSubscribedProperty(char * entityName, char * propertyName, twPrimitive * value, char fold, char pushUpdate);

/**
 * \brief Pushes all queued subscribed properties to the server.
 *
 * \param[in]     entityName        The name of the entity that the properties
 *                                  belongs to.
 * \param[in]     forceConnect      Force a connection if the Thing is currently offline.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
*/
int twApi_PushSubscribedProperties(char * entityName, char forceConnect);

/**
 * \brief Updates the metadata for pre-registered properties.
 *
 * \param[in]     entityType          The type of the entity that the properties
 *                                    belongs to.
 * \param[in]     entityName          The name of the entity that the properties
 *                                    belongs to.
 * \param[in]     propertyName        The name of the property.
 * \param[in]     propertyType        The BaseType of the property.  TW_UNKNOWN_TYPE keeps the current type.
 * \param[in]     propertyDescription A description of the property.  NULL means keep the current description.
 * \param[in]     propertyPushType    The push type of the property.  Can be NEVER, ALWAYS, VALUE (on change).
 * \param[in]     propertyPushThreshold The amount the property has to change (if the type is TW_NUMBER or TW_INTEGER) before pushing the new value.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
*/
int twApi_UpdatePropertyMetaData(enum entityTypeEnum entityType, char * entityName, char * propertyName, enum BaseType propertyType, 
						   char * propertyDescription, char * propertyPushType, double propertyPushThreshold);

/**
 * \name Keep Alive Functions
*/

/* Only use these functions if you want to override default keep alive handling */

/**
 * \brief Pings the ThingWorx server.
 *
 * \param[in]     content           A string to send with the ping.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
 *
 * \note #content must be less than 128 characters.
*/
int twApi_SendPing(char * content);

/**
 * \brief Registers a callback function to be called when a Ping is received
 * (see callback function signature eventcb()).
 *
 * \param[in]     cb                The function to be called.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterPingCallback(eventcb cb);

/**
 * \brief Registers a callback function to be called when a Pong is received
 * (see callback function signature eventcb()).
 *
 * \param[in]     cb                The function to be called.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_RegisterPongCallback(eventcb cb);

/**
 * \name Tasker Functions
*/

/**
 * \brief Adds a new task to the round robin scheduler (see callback function
 * signature twTaskFunction).
 *
 * \param[in]     runTimeIntervalMsec   Time (in milliseconds) to wait between
 *                                      calls to the task function specified.
 * \param[in]     func                  The function to be called.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twApi_CreateTask(uint32_t runTimeIntervalMsec, twTaskFunction func);

#ifdef __cplusplus
}
#endif

#endif /* TW_API_H */
