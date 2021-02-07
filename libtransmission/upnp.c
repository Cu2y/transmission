/*
 * This file Copyright (C) 2007-2014 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
<<<<<<< HEAD
 */

#include <errno.h>
=======
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#include "transmission.h"

#define SSDP_ADDR               "239.255.255.250"
#define SSDP_PORT               1900
#define SSDP_TYPE               "upnp:rootdevice"
#define SSDP_SUBTYPE            "ssdp:alive"
#define SSDP_FIRST_DELAY        3750    /* 3 3/4 seconds */
#define SSDP_MAX_DELAY          1800000 /* 30 minutes */
#define SERVICE_TYPE_IP         "urn:schemas-upnp-org:service:WANIPConnection:1"
#define SERVICE_TYPE_PPP        "urn:schemas-upnp-org:service:WANPPPConnection:1"
#define SOAP_ENVELOPE           "http://schemas.xmlsoap.org/soap/envelope/"
#define LOOP_DETECT_THRESHOLD   10 /* error on 10 add/get/del state changes */
#define MAPPING_CHECK_INTERVAL  900000 /* 15 minutes */
#define HTTP_REQUEST_INTERVAL   500 /* half a second */
#define SOAP_METHOD_NOT_ALLOWED 405
#define IGD_GENERIC_ERROR       500
#define IGD_GENERIC_FAILED      501
#define IGD_NO_MAPPING_EXISTS   714
#define IGD_ADD_CONFLICT        718
#define IGD_NO_DYNAMIC_MAPPING  725

typedef struct tr_upnp_action_s
{
    char * name;
    int    len;
    struct { char * name; char * var; char dir; } * args;
} tr_upnp_action_t;

typedef struct tr_upnp_device_s
{
    char                    * id;
    char                    * host;
    char                    * root;
    int                       port;
    int                       ppp;
    char                    * soap;
    char                    * scpd;
    int                       mappedport;
    char                    * myaddr;
#define UPNPDEV_STATE_ROOT              1
#define UPNPDEV_STATE_SCPD              2
#define UPNPDEV_STATE_READY             3
#define UPNPDEV_STATE_ADD               4
#define UPNPDEV_STATE_GET               5
#define UPNPDEV_STATE_DEL               6
#define UPNPDEV_STATE_MAPPED            7
#define UPNPDEV_STATE_ERROR             8
    uint8_t                   state;
    uint8_t                   looping;
    uint64_t                  lastrequest;
    uint64_t                  lastcheck;
    unsigned int              soapretry : 1;
    tr_http_t               * http;
    tr_upnp_action_t          getcmd;
    tr_upnp_action_t          addcmd;
    tr_upnp_action_t          delcmd;
    struct tr_upnp_device_s * next;
} tr_upnp_device_t;

struct tr_upnp_s
{
    int                port;
    int                infd;
    int                outfd;
    uint64_t           lastdiscover;
    uint64_t           lastdelay;
    unsigned int       active : 1;
    unsigned int       discovering : 1;
    tr_upnp_device_t * devices;
};

static int
sendSSDP( int fd );
static int
mcastStart();
static void
killSock( int * sock );
static void
killHttp( tr_http_t ** http );
static int
watchSSDP( tr_upnp_device_t ** devices, int fd );
static tr_tristate_t
recvSSDP( int fd, char * buf, int * len );
static int
parseSSDP( char * buf, int len, tr_http_header_t * headers );
static void
deviceAdd( tr_upnp_device_t ** first, const char * id, int idLen,
           const char * url, int urlLen );
static void
deviceRemove( tr_upnp_device_t ** prevptr );
static int
deviceStop( tr_upnp_device_t * dev );
static int
devicePulse( tr_upnp_device_t * dev, int port );
static int
devicePulseHttp( tr_upnp_device_t * dev,
                 const char ** body, int * len );
static tr_http_t *
devicePulseGetHttp( tr_upnp_device_t * dev );
static int
parseRoot( const char * root, const char *buf, int len,
           char ** soap, char ** scpd, int * ppp );
static void
addUrlbase( const char * base, char ** path );
static int
parseScpd( const char *buf, int len, tr_upnp_action_t * getcmd,
           tr_upnp_action_t * addcmd, tr_upnp_action_t * delcmd );
static int
parseScpdArgs( const char * buf, const char * end,
               tr_upnp_action_t * action, char dir );
static int
parseMapping( tr_upnp_device_t * dev, const char * buf, int len );
static char *
joinstrs( const char *, const char *, const char * );
static tr_http_t *
soapRequest( int retry, const char * host, int port, const char * path,
             const char * type, tr_upnp_action_t * action, ... );
static void
actionSetup( tr_upnp_action_t * action, const char * name, int prealloc );
static void
actionFree( tr_upnp_action_t * action );
static int
actionAdd( tr_upnp_action_t * action, char * name, char * var,
                      char dir );
#define actionLookupVar( act, nam, len, dir ) \
    ( actionLookup( (act), (nam), (len), (dir), 0 ) )
#define actionLookupName( act, var, len, dir ) \
    ( actionLookup( (act), (var), (len), (dir), 1 ) )
static const char *
actionLookup( tr_upnp_action_t * action, const char * key, int len,
              char dir, int getname );

tr_upnp_t *
tr_upnpInit()
{
    tr_upnp_t * upnp;

    upnp = calloc( 1, sizeof( *upnp ) );
    if( NULL == upnp )
    {
        return NULL;
    }
>>>>>>> origin/0.7x

#ifdef SYSTEM_MINIUPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#else
#include <miniupnp/miniupnpc.h>
#include <miniupnp/upnpcommands.h>
#endif

#include "transmission.h"
#include "log.h"
#include "port-forwarding.h"
#include "session.h"
#include "tr-assert.h"
#include "upnp.h"
#include "utils.h"

static char const* getKey(void)
{
    return _("Port Forwarding (UPnP)");
}

typedef enum
{
    TR_UPNP_IDLE,
    TR_UPNP_ERR,
    TR_UPNP_DISCOVER,
    TR_UPNP_MAP,
    TR_UPNP_UNMAP
}
tr_upnp_state;

struct tr_upnp
{
    bool hasDiscovered;
    struct UPNPUrls urls;
    struct IGDdatas data;
    int port;
    char lanaddr[16];
    bool isMapped;
    tr_upnp_state state;
};

/**
***
**/

tr_upnp* tr_upnpInit(void)
{
    tr_upnp* ret = tr_new0(tr_upnp, 1);

    ret->state = TR_UPNP_DISCOVER;
    ret->port = -1;
    return ret;
}

void tr_upnpClose(tr_upnp* handle)
{
    TR_ASSERT(!handle->isMapped);
    TR_ASSERT(handle->state == TR_UPNP_IDLE || handle->state == TR_UPNP_ERR || handle->state == TR_UPNP_DISCOVER);

    if (handle->hasDiscovered)
    {
        FreeUPNPUrls(&handle->urls);
    }

    tr_free(handle);
}

/**
***  Wrappers for miniupnpc functions
**/

static struct UPNPDev* tr_upnpDiscover(int msec)
{
    struct UPNPDev* ret;
    bool have_err;

#if (MINIUPNPC_API_VERSION >= 8) /* adds ipv6 and error args */
    int err = UPNPDISCOVER_SUCCESS;

#if (MINIUPNPC_API_VERSION >= 14) /* adds ttl */
    ret = upnpDiscover(msec, NULL, NULL, 0, 0, 2, &err);
#else
    ret = upnpDiscover(msec, NULL, NULL, 0, 0, &err);
#endif

    have_err = err != UPNPDISCOVER_SUCCESS;
#else
    ret = upnpDiscover(msec, NULL, NULL, 0);
    have_err = ret == NULL;
#endif

    if (have_err)
    {
        tr_logAddNamedDbg(getKey(), "upnpDiscover failed (errno %d - %s)", errno, tr_strerror(errno));
    }

    return ret;
}

static int tr_upnpGetSpecificPortMappingEntry(tr_upnp* handle, char const* proto)
{
    int err;
    char intClient[16];
    char intPort[16];
    char portStr[16];

    *intClient = '\0';
    *intPort = '\0';

    tr_snprintf(portStr, sizeof(portStr), "%d", handle->port);

#if (MINIUPNPC_API_VERSION >= 10) /* adds remoteHost arg */
    err = UPNP_GetSpecificPortMappingEntry(handle->urls.controlURL, handle->data.first.servicetype, portStr, proto,
        NULL /*remoteHost*/, intClient, intPort, NULL /*desc*/, NULL /*enabled*/, NULL /*duration*/);
#elif (MINIUPNPC_API_VERSION >= 8) /* adds desc, enabled and leaseDuration args */
    err = UPNP_GetSpecificPortMappingEntry(handle->urls.controlURL, handle->data.first.servicetype, portStr, proto, intClient,
        intPort, NULL /*desc*/, NULL /*enabled*/, NULL /*duration*/);
#else
    err = UPNP_GetSpecificPortMappingEntry(handle->urls.controlURL, handle->data.first.servicetype, portStr, proto, intClient,
        intPort);
#endif

    return err;
}

static int tr_upnpAddPortMapping(tr_upnp const* handle, char const* proto, tr_port port, char const* desc)
{
    int err;
    int const old_errno = errno;
    char portStr[16];
    errno = 0;

    tr_snprintf(portStr, sizeof(portStr), "%d", (int)port);

#if (MINIUPNPC_API_VERSION >= 8)
    err = UPNP_AddPortMapping(handle->urls.controlURL, handle->data.first.servicetype, portStr, portStr, handle->lanaddr, desc,
        proto, NULL, NULL);
#else
    err = UPNP_AddPortMapping(handle->urls.controlURL, handle->data.first.servicetype, portStr, portStr, handle->lanaddr, desc,
        proto, NULL);
#endif

    if (err != 0)
    {
        tr_logAddNamedDbg(getKey(), "%s Port forwarding failed with error %d (errno %d - %s)", proto, err, errno,
            tr_strerror(errno));
    }

    errno = old_errno;
    return err;
}

static void tr_upnpDeletePortMapping(tr_upnp const* handle, char const* proto, tr_port port)
{
    char portStr[16];

    tr_snprintf(portStr, sizeof(portStr), "%d", (int)port);

    UPNP_DeletePortMapping(handle->urls.controlURL, handle->data.first.servicetype, portStr, proto, NULL);
}

/**
***
**/

enum
{
<<<<<<< HEAD
    UPNP_IGD_NONE = 0,
    UPNP_IGD_VALID_CONNECTED = 1,
    UPNP_IGD_VALID_NOT_CONNECTED = 2,
    UPNP_IGD_INVALID = 3
};
=======
    const char * body;
    int          len, code;
    uint8_t      laststate;

    switch( dev->state )
    {
        case UPNPDEV_STATE_READY:
            if( 0 < port )
            {
                tr_dbg( "upnp device %s: want mapping, state ready -> get",
                        dev->host );
                dev->mappedport = port;
                dev->state = UPNPDEV_STATE_GET;
                break;
            }
            return 1;
        case UPNPDEV_STATE_MAPPED:
            if( port != dev->mappedport )
            {
                tr_dbg( "upnp device %s: change mapping, "
                        "state mapped -> delete", dev->host );
                dev->state = UPNPDEV_STATE_DEL;
                break;
            }
            if( tr_date() > dev->lastcheck + MAPPING_CHECK_INTERVAL )
            {
                tr_dbg( "upnp device %s: check mapping, "
                        "state mapped -> get", dev->host );
                dev->state = UPNPDEV_STATE_GET;
            }
            return 1;
        case UPNPDEV_STATE_ERROR:
            return 0;
    }

    /* gcc can be pretty annoying about it's warnings sometimes */
    len = 0;
    body = NULL;

    code = devicePulseHttp( dev, &body, &len );
    if( 0 > code )
    {
        return 1;
    }

    if( LOOP_DETECT_THRESHOLD <= dev->looping )
    {
        tr_dbg( "upnp device %s: loop detected, state %hhu -> error",
                dev->host, dev->state );
        dev->state = UPNPDEV_STATE_ERROR;
        dev->looping = 0;
        killHttp( &dev->http );
        return 1;
    }

    laststate = dev->state;
    dev->state = UPNPDEV_STATE_ERROR;
    switch( laststate ) 
    {
        case UPNPDEV_STATE_ROOT:
            if( !TR_HTTP_STATUS_OK( code ) )
            {
                tr_dbg( "upnp device %s: fetch root failed with http code %i",
                        dev->host, code );
            }
            else if( parseRoot( dev->root, body, len,
                                &dev->soap, &dev->scpd, &dev->ppp ) )
            {
                tr_dbg( "upnp device %s: parse root failed", dev->host );
            }
            else
            {
                tr_dbg( "upnp device %s: found scpd \"%s\" and soap \"%s\"",
                        dev->root, dev->scpd, dev->soap );
                tr_dbg( "upnp device %s: parsed root, state root -> scpd",
                        dev->host );
                dev->state = UPNPDEV_STATE_SCPD;
            }
            break;

        case UPNPDEV_STATE_SCPD:
            if( !TR_HTTP_STATUS_OK( code ) )
            {
                tr_dbg( "upnp device %s: fetch scpd failed with http code %i",
                        dev->host, code );
            }
            else if( parseScpd( body, len, &dev->getcmd,
                                &dev->addcmd, &dev->delcmd ) )
            {
                tr_dbg( "upnp device %s: parse scpd failed", dev->host );
            }
            else
            {
                tr_dbg( "upnp device %s: parsed scpd, state scpd -> ready",
                        dev->host );
                dev->state = UPNPDEV_STATE_READY;
                dev->looping = 0;
            }
            break;

        case UPNPDEV_STATE_ADD:
            dev->looping++;
            if( IGD_ADD_CONFLICT == code )
            {
                tr_dbg( "upnp device %s: add conflict, state add -> delete",
                        dev->host );
                dev->state = UPNPDEV_STATE_DEL;
            }
            else if( TR_HTTP_STATUS_OK( code ) ||
                     IGD_GENERIC_ERROR == code || IGD_GENERIC_FAILED == code )
            {
                tr_dbg( "upnp device %s: add attempt, state add -> get",
                        dev->host );
                dev->state = UPNPDEV_STATE_GET;
            }
            else
            {
                tr_dbg( "upnp device %s: add failed with http code %i",
                        dev->host, code );
            }
            break;

        case UPNPDEV_STATE_GET:
            dev->looping++;
            if( TR_HTTP_STATUS_OK( code ) )
            {
                switch( parseMapping( dev, body, len ) )
                {
                    case -1:
                        break;
                    case 0:
                        tr_dbg( "upnp device %s: invalid mapping, "
                                "state get -> delete", dev->host );
                        dev->state = UPNPDEV_STATE_DEL;
                        break;
                    case 1:
                        tr_dbg( "upnp device %s: good mapping, "
                                "state get -> mapped", dev->host );
                        dev->state = UPNPDEV_STATE_MAPPED;
                        dev->looping = 0;
                        dev->lastcheck = tr_date();
                        tr_inf( "upnp successful for port %i",
                                dev->mappedport );
                        break;
                    default:
                        assert( 0 );
                        break;
                }
            }
            else if( IGD_NO_MAPPING_EXISTS == code ||
                     IGD_GENERIC_ERROR == code || IGD_GENERIC_FAILED == code )
            {
                tr_dbg( "upnp device %s: no mapping, state get -> add",
                        dev->host );
                dev->state = UPNPDEV_STATE_ADD;
            }
            else
            {
                tr_dbg( "upnp device %s: get failed with http code %i",
                        dev->host, code );
            }
            break;

        case UPNPDEV_STATE_DEL:
            dev->looping++;
            if( TR_HTTP_STATUS_OK( code ) || IGD_NO_MAPPING_EXISTS == code ||
                IGD_GENERIC_ERROR == code || IGD_GENERIC_FAILED == code )
            {
                tr_dbg( "upnp device %s: deleted, state delete -> ready",
                        dev->host );
                dev->state = UPNPDEV_STATE_READY;
                dev->looping = 0;
            }
            else
            {
                tr_dbg( "upnp device %s: del failed with http code %i",
                        dev->host, code );
            }
            break;

        default:
            assert( 0 );
            break;
    }

    dev->lastrequest = tr_date();
    killHttp( &dev->http );

    if( UPNPDEV_STATE_ERROR == dev->state )
    {
        tr_dbg( "upnp device %s: error, state %hhu -> error",
                dev->host, laststate );
        return 0;
    }

    return 1;
}

static tr_http_t *
makeHttp( int method, const char * host, int port, const char * path )
{
    if( tr_httpIsUrl( path, -1 ) )
    {
        return tr_httpClientUrl( method, "%s", path );
    }
    else
    {
        return tr_httpClient( method, host, port, "%s", path );
    }
}
>>>>>>> origin/0.7x

int tr_upnpPulse(tr_upnp* handle, tr_port port, bool isEnabled, bool doPortCheck)
{
<<<<<<< HEAD
    int ret;
=======
    tr_http_t  * ret;
    char         numstr[6];
    const char * type;
>>>>>>> origin/0.7x

    if (isEnabled && handle->state == TR_UPNP_DISCOVER)
    {
<<<<<<< HEAD
        struct UPNPDev* devlist;
=======
        case UPNPDEV_STATE_ROOT:
            if( !dev->soapretry )
            {
                ret = makeHttp( TR_HTTP_GET, dev->host, dev->port, dev->root );
            }
            break;
        case UPNPDEV_STATE_SCPD:
            if( !dev->soapretry )
            {
                ret = makeHttp( TR_HTTP_GET, dev->host, dev->port, dev->scpd );
            }
            break;
        case UPNPDEV_STATE_ADD:
            if( NULL == dev->myaddr )
            {
                ret = NULL;
                break;
            }
            snprintf( numstr, sizeof( numstr ), "%i", dev->mappedport );
            type = ( dev->ppp ? SERVICE_TYPE_PPP : SERVICE_TYPE_IP );
            ret = soapRequest( dev->soapretry, dev->host, dev->port, dev->soap,
                               type, &dev->addcmd,
                               "PortMappingEnabled", "1",
                               "PortMappingLeaseDuration", "0",
                               "RemoteHost", "",
                               "ExternalPort", numstr,
                               "InternalPort", numstr,
                               "PortMappingProtocol", "TCP",
                               "InternalClient", dev->myaddr,
                               "PortMappingDescription", "Added by " TR_NAME,
                               NULL );
            break;
        case UPNPDEV_STATE_GET:
            snprintf( numstr, sizeof( numstr ), "%i", dev->mappedport );
            type = ( dev->ppp ? SERVICE_TYPE_PPP : SERVICE_TYPE_IP );
            ret = soapRequest( dev->soapretry, dev->host, dev->port, dev->soap,
                               type, &dev->getcmd,
                               "RemoteHost", "",
                               "ExternalPort", numstr,
                               "PortMappingProtocol", "TCP",
                               NULL );
            break;
        case UPNPDEV_STATE_DEL:
            snprintf( numstr, sizeof( numstr ), "%i", dev->mappedport );
            type = ( dev->ppp ? SERVICE_TYPE_PPP : SERVICE_TYPE_IP );
            ret = soapRequest( dev->soapretry, dev->host, dev->port, dev->soap,
                               type, &dev->delcmd,
                               "RemoteHost", "",
                               "ExternalPort", numstr,
                               "PortMappingProtocol", "TCP",
                               NULL );
            break;
        default:
            assert( 0 );
            break;
    }
>>>>>>> origin/0.7x

        devlist = tr_upnpDiscover(2000);

        errno = 0;

        if (UPNP_GetValidIGD(devlist, &handle->urls, &handle->data, handle->lanaddr,
            sizeof(handle->lanaddr)) == UPNP_IGD_VALID_CONNECTED)
        {
            tr_logAddNamedInfo(getKey(), _("Found Internet Gateway Device \"%s\""), handle->urls.controlURL);
            tr_logAddNamedInfo(getKey(), _("Local Address is \"%s\""), handle->lanaddr);
            handle->state = TR_UPNP_IDLE;
            handle->hasDiscovered = true;
        }
        else
        {
            handle->state = TR_UPNP_ERR;
            tr_logAddNamedDbg(getKey(), "UPNP_GetValidIGD failed (errno %d - %s)", errno, tr_strerror(errno));
            tr_logAddNamedDbg(getKey(), "If your router supports UPnP, please make sure UPnP is enabled!");
        }

        freeUPNPDevlist(devlist);
    }

<<<<<<< HEAD
    if ((handle->state == TR_UPNP_IDLE) &&
        (handle->isMapped) &&
        (!isEnabled || handle->port != port))
    {
        handle->state = TR_UPNP_UNMAP;
=======
    return -1;
}

static int
parseRoot( const char * root, const char *buf, int len,
           char ** soap, char ** scpd, int * ppp )
{
    const char * end, * ii, * jj, * kk, * urlbase;
    char       * basedup;

    *soap = NULL;
    *scpd = NULL;
    end = buf + len;

    buf = tr_xmlFindTagContents( buf, end, "root" );
    urlbase = tr_xmlFindTag( buf, end, "urlBase" );
    urlbase = tr_xmlTagContents( urlbase, end );
    buf = tr_xmlFindTagContents( buf, end, "device" );
    if( tr_xmlFindTagVerifyContents( buf, end, "deviceType",
            "urn:schemas-upnp-org:device:InternetGatewayDevice:1", 1 ) )
    {
        return 1;
    }
    buf = tr_xmlFindTag( buf, end, "deviceList" );
    ii = tr_xmlTagContents( buf, end );
    for( ; NULL != ii; ii = tr_xmlSkipTag( ii, end ) )
    {
        ii = tr_xmlFindTag( ii, end, "device" );
        buf = tr_xmlTagContents( ii, end );
        if( tr_xmlFindTagVerifyContents( buf, end, "deviceType",
                "urn:schemas-upnp-org:device:WANDevice:1", 1 ) )
        {
            continue;
        }
        buf = tr_xmlFindTag( buf, end, "deviceList" );
        jj = tr_xmlTagContents( buf, end );
        for( ; NULL != jj; jj = tr_xmlSkipTag( jj, end ) )
        {
            jj = tr_xmlFindTag( jj, end, "device" );
            buf = tr_xmlTagContents( jj, end );
            if( tr_xmlFindTagVerifyContents( buf, end, "deviceType",
                    "urn:schemas-upnp-org:device:WANConnectionDevice:1", 1 ) )
            {
                continue;
            }
            buf = tr_xmlFindTag( buf, end, "serviceList" );
            kk = tr_xmlTagContents( buf, end );
            for( ; NULL != kk; kk = tr_xmlSkipTag( kk, end ) )
            {
                kk = tr_xmlFindTag( kk, end, "service" );
                buf = tr_xmlTagContents( kk, end );
                if( !tr_xmlFindTagVerifyContents( buf, end, "serviceType",
                                                  SERVICE_TYPE_IP, 1 ) )
                {
                    *soap = tr_xmlDupTagContents( buf, end, "controlURL");
                    *scpd = tr_xmlDupTagContents( buf, end, "SCPDURL");
                    *ppp  = 0;
                    break;
                }
                /* XXX we should save all services of both types and
                   try adding mappings for each in turn */
                else if( !tr_xmlFindTagVerifyContents( buf, end, "serviceType",
                                                       SERVICE_TYPE_PPP, 1 ) )
                {
                    *soap = tr_xmlDupTagContents( buf, end, "controlURL");
                    *scpd = tr_xmlDupTagContents( buf, end, "SCPDURL");
                    *ppp  = 1;
                    break;
                }
            }
        }
>>>>>>> origin/0.7x
    }

    if (isEnabled &&
        handle->isMapped &&
        doPortCheck &&
        ((tr_upnpGetSpecificPortMappingEntry(handle, "TCP") != UPNPCOMMAND_SUCCESS) ||
        (tr_upnpGetSpecificPortMappingEntry(handle, "UDP") != UPNPCOMMAND_SUCCESS)))
    {
        tr_logAddNamedInfo(getKey(), _("Port %d isn't forwarded"), handle->port);
        handle->isMapped = false;
    }

    if (handle->state == TR_UPNP_UNMAP)
    {
        tr_upnpDeletePortMapping(handle, "TCP", handle->port);
        tr_upnpDeletePortMapping(handle, "UDP", handle->port);

        tr_logAddNamedInfo(getKey(), _("Stopping port forwarding through \"%s\", service \"%s\""), handle->urls.controlURL,
            handle->data.first.servicetype);

        handle->isMapped = false;
        handle->state = TR_UPNP_IDLE;
        handle->port = -1;
    }

    if ((handle->state == TR_UPNP_IDLE) && isEnabled && !handle->isMapped)
    {
        handle->state = TR_UPNP_MAP;
    }

    if (handle->state == TR_UPNP_MAP)
    {
        errno = 0;

        if (handle->urls.controlURL == NULL)
        {
            handle->isMapped = false;
        }
        else
        {
            char desc[64];
            tr_snprintf(desc, sizeof(desc), "%s at %d", TR_NAME, port);

            int const err_tcp = tr_upnpAddPortMapping(handle, "TCP", port, desc);
            int const err_udp = tr_upnpAddPortMapping(handle, "UDP", port, desc);

            handle->isMapped = err_tcp == 0 || err_udp == 0;
        }

<<<<<<< HEAD
        tr_logAddNamedInfo(getKey(), _("Port forwarding through \"%s\", service \"%s\". (local address: %s:%d)"),
            handle->urls.controlURL, handle->data.first.servicetype, handle->lanaddr, port);

        if (handle->isMapped)
=======
static tr_http_t *
soapRequest( int retry, const char * host, int port, const char * path,
             const char * type, tr_upnp_action_t * action, ... )
{
    tr_http_t  * http;
    va_list      ap;
    const char * name, * value;
    int          method;
    char       * actstr;

    method = ( retry ? TR_HTTP_M_POST : TR_HTTP_POST );
    http = makeHttp( method, host, port, path );
    if( NULL != http )
    {
        tr_httpAddHeader( http, "Content-type",
                          "text/xml; encoding=\"utf-8\"" );
        actstr = NULL;
        asprintf( &actstr, "\"%s#%s\"", type, action->name );
        tr_httpAddHeader( http, "SOAPAction", actstr );
        free( actstr );
        if( retry )
>>>>>>> origin/0.7x
        {
            tr_logAddNamedInfo(getKey(), "%s", _("Port forwarding successful!"));
            handle->port = port;
            handle->state = TR_UPNP_IDLE;
        }
<<<<<<< HEAD
        else
=======
        tr_httpAddBody( http, 
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<s:Envelope"
"    xmlns:s=\"" SOAP_ENVELOPE "\""
"    s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
"  <s:Body>"
"    <u:%s xmlns:u=\%s\">", action->name, type );

        va_start( ap, action );
        do
>>>>>>> origin/0.7x
        {
            tr_logAddNamedDbg(getKey(), "If your router supports UPnP, please make sure UPnP is enabled!");
            handle->port = -1;
            handle->state = TR_UPNP_ERR;
        }
    }

<<<<<<< HEAD
    switch (handle->state)
=======
    return http;
}

static void
actionSetup( tr_upnp_action_t * action, const char * name, int prealloc )
{
    action->name = strdup( name );
    assert( NULL == action->args );
    action->args = malloc( sizeof( *action->args ) * prealloc );
    memset( action->args, 0, sizeof( *action->args ) * prealloc );
    action->len = prealloc;
}

static void
actionFree( tr_upnp_action_t * act )
{
    free( act->name );
    while( 0 < act->len )
>>>>>>> origin/0.7x
    {
    case TR_UPNP_DISCOVER:
        ret = TR_PORT_UNMAPPED;
        break;

    case TR_UPNP_MAP:
        ret = TR_PORT_MAPPING;
        break;

    case TR_UPNP_UNMAP:
        ret = TR_PORT_UNMAPPING;
        break;

    case TR_UPNP_IDLE:
        ret = handle->isMapped ? TR_PORT_MAPPED : TR_PORT_UNMAPPED;
        break;

    default:
        ret = TR_PORT_ERROR;
        break;
    }

    return ret;
}

#if 0
/* this code is used for standalone root parsing for debugging purposes */
/* cc -g -Wall -D__TRANSMISSION__ -o upnp upnp.c xml.c utils.c */
int
main( int argc, char * argv[] )
{
    struct stat sb;
    char      * data, * soap, * scpd;
    int         fd, ppp;
    ssize_t     res;

    if( 3 != argc )
    {
        printf( "usage: %s root-url root-file\n", argv[0] );
        return 0;
    }

    tr_msgInit();
    tr_setMessageLevel( 9 );

    if( 0 > stat( argv[2], &sb ) )
    {
        tr_err( "failed to stat file %s: %s", argv[2], strerror( errno ) );
        return 1;
    }

    data = malloc( sb.st_size );
    if( NULL == data )
    {
        tr_err( "failed to malloc %zd bytes", ( size_t )sb.st_size );
        return 1;
    }

    fd = open( argv[2], O_RDONLY );
    if( 0 > fd )
    {
        tr_err( "failed to open file %s: %s", argv[2], strerror( errno ) );
        free( data );
        return 1;
    }

    res = read( fd, data, sb.st_size );
    if( sb.st_size > res )
    {
        tr_err( "failed to read file %s: %s", argv[2],
                ( 0 > res ? strerror( errno ) : "short read count" ) );
        close( fd );
        free( data );
        return 1;
    }

    close( fd );

    if( parseRoot( argv[1], data, sb.st_size, &soap, &scpd, &ppp ) )
    {
        tr_err( "root parsing failed" );
    }
    else
    {
        tr_err( "soap=%s scpd=%s ppp=%s", soap, scpd, ( ppp ? "yes" : "no" ) );
        free( soap );
        free( scpd );
    }
    free( data );

    return 0;
}

int  tr_netMcastOpen( int port, struct in_addr addr ) { assert( 0 ); }
int  tr_netBind    ( int port, int type ) { assert( 0 ); }
void tr_netClose   ( int s ) { assert( 0 ); }
int  tr_netRecvFrom( int s, uint8_t * buf, int size, struct sockaddr_in * sin ) { assert( 0 ); }
int         tr_httpRequestType( const char * data, int len,
                                char ** method, char ** uri ) { assert( 0 ); }
int         tr_httpResponseCode( const char * data, int len ) { assert( 0 ); }
char *      tr_httpParse( const char * data, int len, tr_http_header_t *headers ) { assert( 0 ); }
int         tr_httpIsUrl( const char * u, int l ) { assert( 0 ); }
int         tr_httpParseUrl( const char * u, int l, char ** h, int * p, char ** q ) { assert( 0 ); }
tr_http_t   * tr_httpClient( int t, const char * h, int p, const char * u, ... ) { assert( 0 ); }
tr_http_t   * tr_httpClientUrl( int t, const char * u, ... ) { assert( 0 ); }
void          tr_httpAddHeader( tr_http_t * h, const char * n, const char * v ) { assert( 0 ); }
void          tr_httpAddBody( tr_http_t * h, const char * b, ... ) { assert( 0 ); }
tr_tristate_t tr_httpPulse( tr_http_t * h, const char ** b, int * l ) { assert( 0 ); }
char        * tr_httpWhatsMyAddress( tr_http_t * h ) { assert( 0 ); }
void          tr_httpClose( tr_http_t * h ) { assert( 0 ); }

void tr_lockInit     ( tr_lock_t * l ) {}
int  pthread_mutex_lock( pthread_mutex_t * m ) { return 0; }
int  pthread_mutex_unlock( pthread_mutex_t * m ) { return 0; }

#endif
