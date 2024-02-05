#define _CRT_RAND_S

#pragma once

//-----------------------------------------------------------------------------------------------------------------
//
// includes
//
//-----------------------------------------------------------------------------------------------------------------
#include "../FSXInc/gauges.h"
#include "../FSXInc/gps_info.h"
#include "../FSXInc/SimConnect.h"

#include <wchar.h>
#include <stdio.h>
#include <math.h>
#include <strsafe.h>
#include <WinError.h>
#include <winhttp.h>


//-----------------------------------------------------------------------------------------------------------------
//
// global variables
//
//-----------------------------------------------------------------------------------------------------------------
HRESULT hr = 0;
HANDLE  hSimConnect = 0;

char gICAO[4];
char gMETAR[MAX_METAR_LENGTH];

int weatherInit = 0;


void WrapMetar35(const char* m);


//-----------------------------------------------------------------------------------------------------------------
//
// SimConnect stuff
//
//-----------------------------------------------------------------------------------------------------------------
struct s_airplaneData
{
    char    title[256];
    double  kohlsmann;
    double  altitude;
    double  latitude;
    double  longitude;
};

s_airplaneData *pAirplaneData = 0;

static enum eventId
{
    eventSimStart,
};

static enum airplaneDefineId
{
    airplaneDataDefinition,
};

static enum dataRequestId
{
    requestAirplaneData,
    requestWeather,
};

//-----------------------------------------------------------------------------------------------------------------
//
// SimConnect event processor
//
//-----------------------------------------------------------------------------------------------------------------
void CALLBACK eventProcessor(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext)
{
    switch(pData->dwID)
    {
        case SIMCONNECT_RECV_ID_EVENT:
        {
            SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;

            switch(evt->uEventID)
            {
                case eventSimStart:
				{   
					// request the location of the airplane just once for initializing
					hr = SimConnect_RequestDataOnSimObject(hSimConnect,
						requestAirplaneData,
						airplaneDataDefinition,
						SIMCONNECT_OBJECT_ID_USER,
						SIMCONNECT_PERIOD_ONCE);

					break;
				}


                default:
				{
                   break;
				}
            }

            break;
        }

        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        {
            SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;
            
            switch(pObjData->dwRequestID)
            {
                case requestAirplaneData:
                {
                    DWORD ObjectID = pObjData->dwObjectID;
                    pAirplaneData = (s_airplaneData*)&pObjData->dwData;

					if (weatherInit == 0)
					{
						weatherInit++;
						hr = SimConnect_WeatherRequestObservationAtNearestStation(hSimConnect,
							requestWeather,
							pAirplaneData->latitude,
							pAirplaneData->longitude);
					}

                    break;
                }

                default:
				{
                   break;
				}
            }
            
			break;
        }

		case SIMCONNECT_RECV_ID_WEATHER_OBSERVATION:
        {
            SIMCONNECT_RECV_WEATHER_OBSERVATION* pWxData = (SIMCONNECT_RECV_WEATHER_OBSERVATION*) pData;

            const char* pszMETAR = pWxData->szMetar;

			switch(pWxData->dwRequestID)
            {
				case requestWeather:
				{
					if (weatherInit == 1)
					{
						weatherInit++;
						sprintf(gICAO, "%c%c%c%c", pszMETAR[0], pszMETAR[1], pszMETAR[2], pszMETAR[3]);
					}

					break;
				}

				default:
				{

					break;
				}
			}
		}
        
		default:
		{
            break;
		}
    }
}


void WrapMetar35(const char* a)
{
	int l = strlen(a) + 8;
	int m = 0;
	for(int i=0;i<l;i++)
	{
		if (i==34 || i==69 || i==104 || i==139)
		{
			gMETAR[m] = '\\';
			gMETAR[m+1] = 'n';
			m+=2;
		}

		gMETAR[m] = a[i];
		m++;
	}
}

//-----------------------------------------------------------------------------------------------------------------
//
// Initialize SimConnect
//
//-----------------------------------------------------------------------------------------------------------------
void InitializeSimConnect()
{
    if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Request Data", NULL, 0, 0, 0)))
    {
        // Set up the data definition
        hr = SimConnect_AddToDataDefinition(hSimConnect, airplaneDataDefinition, "Title", NULL, SIMCONNECT_DATATYPE_STRING256);
        hr = SimConnect_AddToDataDefinition(hSimConnect, airplaneDataDefinition, "Kohlsman setting hg", "inHg");
        hr = SimConnect_AddToDataDefinition(hSimConnect, airplaneDataDefinition, "Plane Altitude", "feet");
        hr = SimConnect_AddToDataDefinition(hSimConnect, airplaneDataDefinition, "Plane Latitude", "degrees");
        hr = SimConnect_AddToDataDefinition(hSimConnect, airplaneDataDefinition, "Plane Longitude", "degrees");

        // Request event eventSimStart when the simulation starts
        hr = SimConnect_SubscribeToSystemEvent(hSimConnect, eventSimStart, "SimStart");
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// Gauge Specific Items
//
//-----------------------------------------------------------------------------------------------------------------

// Note: The items in the property table correspond to the indices that
// will be returned in the Get/Set Property functions
struct PROPERTY_TABLE
{
    PCSTRINGZ szPropertyName;
	PCSTRINGZ szUnitsName;
    ENUM units;
};

//-----------------------------------------------------------------------------------------------------------------
//
// Panel class
//
//-----------------------------------------------------------------------------------------------------------------
class PanelCallback : public IPanelCCallback
{
    DECLARE_PANEL_CALLBACK_REFCOUNT(PanelCallback);
    
public:

	PanelCallback();
    
    IPanelCCallback* QueryInterface(PCSTRINGZ pszInterface);    
    
	UINT32 GetVersion();

	bool ConvertStringToProperty (PCSTRINGZ keyword, SINT32* pID);
    bool ConvertPropertyToString (SINT32 id, PPCSTRINGZ pKeyword);
    bool GetPropertyUnits (SINT32 id, ENUM* pEnum);

protected:

    virtual const PROPERTY_TABLE *GetPropertyTable(UINT &uLength) = 0;

};

//-----------------------------------------------------------------------------------------------------------------
//
// Panel Methods
//
//-----------------------------------------------------------------------------------------------------------------

DEFINE_PANEL_CALLBACK_REFCOUNT(PanelCallback);

PanelCallback::PanelCallback() : m_RefCount(1)
{
}
 
IPanelCCallback* PanelCallback::QueryInterface(PCSTRINGZ pszInterface)
{
    return NULL;
}

UINT32 PanelCallback::GetVersion()
{
    return 1;
}

bool PanelCallback::ConvertStringToProperty (PCSTRINGZ keyword, SINT32* pID)
{
    if (!keyword)
    {
        return false;
    }
    if (!pID)
    {
        return false;
    }

    UINT uNumProperties;
    const PROPERTY_TABLE *parPropertyTable = GetPropertyTable(uNumProperties);
    
    for(UINT i = 0; i < uNumProperties; i++)
    {
		if (_stricmp(parPropertyTable[i].szPropertyName, keyword) == 0)
        {
            *pID = i;
            return true;
        }
    }
    return false;         
}

bool PanelCallback::ConvertPropertyToString (SINT32 id, PPCSTRINGZ pKeyword)
{
    if (!pKeyword)
    {
        return false;
    }
    
    UINT uNumProperties;
    const PROPERTY_TABLE *parPropertyTable = GetPropertyTable(uNumProperties);

    if ((id < 0) || (id >= (SINT32)uNumProperties))
    {
        return false;
    }

    *pKeyword = parPropertyTable[id].szPropertyName;
    return true; 
}

bool PanelCallback::GetPropertyUnits (SINT32 id, ENUM* pEnum)
{
    if (!pEnum)
    {
        return false;
    }

    UINT uNumProperties;
    const PROPERTY_TABLE *parPropertyTable = GetPropertyTable(uNumProperties);
    
    if ((id < 0) || (id >= (SINT32)uNumProperties))
    {
        return false;
    }

    *pEnum = parPropertyTable[id].units;
    return true;
}

//-----------------------------------------------------------------------------------------------------------------
//
// Aircraft Class
//
//-----------------------------------------------------------------------------------------------------------------
class AircraftCallback : public IAircraftCCallback 
{
    DECLARE_PANEL_CALLBACK_REFCOUNT(AircraftCallback);

public:

	AircraftCallback(UINT32 containerId);
    IAircraftCCallback* QueryInterface(PCSTRINGZ pszInterface);

	void Update();

protected:    
    
	UINT32 GetContainerId() const;

private:
    
	UINT32 containerId;
};

//-----------------------------------------------------------------------------------------------------------------
//
// Aircraft Methods
//
//-----------------------------------------------------------------------------------------------------------------
DEFINE_PANEL_CALLBACK_REFCOUNT(AircraftCallback);

AircraftCallback::AircraftCallback(UINT32 containerId) : containerId(containerId), m_RefCount(1)
{
}

IAircraftCCallback* AircraftCallback::QueryInterface(PCSTRINGZ pszInterface)
{
    return NULL;
}

void AircraftCallback::Update()
{
	SimConnect_CallDispatch(hSimConnect, eventProcessor, NULL);
}

UINT32 AircraftCallback::GetContainerId() const
{
    return containerId;
}

//-----------------------------------------------------------------------------------------------------------------
//
// Gauge Variables
//
//-----------------------------------------------------------------------------------------------------------------
static const char callbackName[] = "METARGAUGE";

int totalVars = 7;

// XML Variable Names
static PROPERTY_TABLE CABIN_PROPERTY_TABLE[] = 
{
	{ "ICAO",						"Degrees", 		UNITS_STRING},
	{ "ICAO1",						"Degrees", 		UNITS_UNKNOWN},
	{ "ICAO2",						"Degrees", 		UNITS_UNKNOWN},
	{ "ICAO3",						"Degrees", 		UNITS_UNKNOWN},
	{ "ICAO4",						"Degrees", 		UNITS_UNKNOWN},
	{ "GETMETAR",					"Degrees", 		UNITS_UNKNOWN},
	{ "METAR",						"String", 		UNITS_STRING},
};

// Enum that contains the properties
// entries relative to property table
enum GAUGE_VAR
{
	GAUGE_VAR_ICAO,
	GAUGE_VAR_ICAO1,
	GAUGE_VAR_ICAO2,
	GAUGE_VAR_ICAO3,
	GAUGE_VAR_ICAO4,
	GAUGE_VAR_GETMETAR,
	GAUGE_VAR_METAR,
};

//-----------------------------------------------------------------------------------------------------------------
//
// Gauge Class
//
//-----------------------------------------------------------------------------------------------------------------
class GaugeCallback : public IGaugeCCallback
{
    DECLARE_PANEL_CALLBACK_REFCOUNT(GaugeCallback);
	
public:

	GaugeCallback(UINT32 containerId);	
    
    IGaugeCCallback* QueryInterface(PCSTRINGZ pszInterface);
    
	void Update();
    
	bool GetPropertyValue (SINT32 id, FLOAT64* pValue);
    bool GetPropertyValue (SINT32 id, PCSTRINGZ* pszValue);
    bool SetPropertyValue (SINT32 id, FLOAT64 value);
    bool SetPropertyValue (SINT32 id, PCSTRINGZ szValue);
    
	IGaugeCDrawable* CreateGaugeCDrawable(SINT32 id, const IGaugeCDrawableCreateParameters* pParameters);

	int GaugeCallback::getICAO1();
	int GaugeCallback::getICAO2();
	int GaugeCallback::getICAO3();
	int GaugeCallback::getICAO4();

	int GaugeCallback::CheckICAORange(int i, int v);

	void GaugeCallback::GetXMLMETAR();

private:

	UINT32 containerId;

	char	ICAO1;
	char	ICAO2;
	char	ICAO3;
	char	ICAO4;

	char buffer[1024];

};

//-----------------------------------------------------------------------------------------------------------------
//
// Gauge Methods
//
//-----------------------------------------------------------------------------------------------------------------
int GaugeCallback::getICAO1()
{
	return ICAO1;
}

int GaugeCallback::getICAO2()
{
	return ICAO2;
}

int GaugeCallback::getICAO3()
{
	return ICAO3;
}

int GaugeCallback::getICAO4()
{
	return ICAO4;
}

DEFINE_PANEL_CALLBACK_REFCOUNT(GaugeCallback)

//
// initialize gauge variables
//
GaugeCallback::GaugeCallback(UINT32 containerId) : m_RefCount(1), containerId(containerId)
{
	return;
}

//
// return string not allowed
//
IGaugeCCallback* GaugeCallback::QueryInterface(PCSTRINGZ pszInterface)
{
    return NULL;
}

void GaugeCallback::Update()
{
	return;
}

//
// Get float properties
//
bool GaugeCallback::GetPropertyValue (SINT32 id, FLOAT64* pValue)
{
    if (!pValue)
    {
       return false;
    }

    *pValue = 1.0;
   
    GAUGE_VAR eGaugeVar = (GAUGE_VAR)id;

    switch(eGaugeVar)
    {
		case GAUGE_VAR_GETMETAR:
		{
			GaugeCallback::GetXMLMETAR();
	        break;
		}

		case GAUGE_VAR_ICAO1:
		{
			*pValue = getICAO1();
	        break;
		}

		case GAUGE_VAR_ICAO2:
		{
			*pValue = getICAO2();
	        break;
		}

		case GAUGE_VAR_ICAO3:
		{
			*pValue = getICAO3();
	        break;
		}

		case GAUGE_VAR_ICAO4:
		{
			*pValue = getICAO4();
	        break;
		}

		default:
		{
			return false;
		}
    }

    return true; 
}

//
// Set integer properties
//
bool GaugeCallback::GetPropertyValue (SINT32 id, PCSTRINGZ* pszValue)
{
    if (!pszValue)
    {
       return false;
    }

    *pszValue = "null";
    
    GAUGE_VAR eGaugeVar = (GAUGE_VAR)id;

    switch(eGaugeVar)
    {
		case GAUGE_VAR_ICAO:
		{
			memset(&buffer,0x00,1024);

			if (weatherInit == 2)
			{
				weatherInit++;
				
				sprintf(buffer, "%s", gICAO);
				
				ICAO1 = gICAO[0];
				ICAO2 = gICAO[1];
				ICAO3 = gICAO[2];
				ICAO4 = gICAO[3];

				GaugeCallback::GetXMLMETAR();

			}
			else
			{
				sprintf(buffer, "%c%c%c%c", ICAO1, ICAO2, ICAO3, ICAO4);
			}

			*pszValue = buffer;

			break;
		}

		case GAUGE_VAR_METAR:
		{
			*pszValue = gMETAR;
			
			break;
		}

		default:
		{
			return false;
		}
    }

    return true; 
}


int GaugeCallback::CheckICAORange(int i, int v)
{
	if(v < i)
	{
		if(v < 65)
		{
			if (v < 48)
			{
				return 90;
			}
			else
			if (i > 57)
			{
				return 57;
			}
		}
	}

	if(v > i)
	{
		if(v > 57)
		{
			if (v > 90)
			{
				return 48;
			}
			else
			if (i < 58)
			{
				return 65;
			}
		}
	}

	return v;
}

void GaugeCallback::GetXMLMETAR()
{
	// HTTP request to adds weather text data server for metars
	BOOL  bResults = FALSE;
	HINTERNET  hSession = NULL, 
	hConnect = NULL,
	hRequest = NULL;

	// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(L"WinHTTP Example/1.0",  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,	WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	// Specify an HTTP server.
	if (hSession)
	{
		hConnect = WinHttpConnect(hSession, L"www.aviationweather.gov", INTERNET_DEFAULT_HTTP_PORT, 0);
	}

	// Create an HTTP request handle.
	// www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&stationString=KORD&hoursBeforeNow=1

	wchar_t pszURL[256];
	memset(pszURL, 0x00, 256);
	swprintf(pszURL, L"/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&stationString=%c%c%c%c&hoursBeforeNow=1", ICAO1, ICAO2, ICAO3, ICAO4);

	if (hConnect)
	{
		hRequest = WinHttpOpenRequest(hConnect, L"GET", pszURL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, NULL);
	}

	// Send a request.
	if (hRequest)
	{
		bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	}

	// End the request.
	if (bResults)
	{
		bResults = WinHttpReceiveResponse(hRequest, NULL);
	}

	LPSTR pszOutBuffer = new char[32768];
	memset(pszOutBuffer, 0x00, 32768);

	DWORD dwSize = 0;

	// If some data was returned ...
	if (bResults)
	{
		if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
		{
			sprintf(buffer, "Error %u in WinHttpQueryDataAvailable.\n", GetLastError());
		}

		DWORD dwDownloaded = 0;
		pszOutBuffer = new char[dwSize+1];
		memset(pszOutBuffer, 0x00, dwSize+1);
				
		WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded);
	}

	// Report any errors.
	if (!bResults)
	{
		sprintf(buffer, "Error %d has occurred.\n", GetLastError());
	}

	// Close any open handles.
	if (hRequest)
	{
		WinHttpCloseHandle(hRequest);
	}
	if (hConnect)
	{
		WinHttpCloseHandle(hConnect);
	}
	if (hSession)
	{
		WinHttpCloseHandle(hSession);
	}

	int a = 0;
	char metar[256];
	memset(metar, 0x00, 256);
	memset(gMETAR, 0x00, MAX_METAR_LENGTH);

	for(int i=0;i<dwSize-9;i++)
	{
		if ((pszOutBuffer[i + 0] == '<') && (pszOutBuffer[i + 1] == 'r') && (pszOutBuffer[i + 2] == 'a') && (pszOutBuffer[i + 3] == 'w') && (pszOutBuffer[i + 4] == '_') && (pszOutBuffer[i + 5] == 't') && (pszOutBuffer[i + 6] == 'e') && (pszOutBuffer[i + 7] == 'x') && (pszOutBuffer[i + 8] == 't') && (pszOutBuffer[i + 9] == '>'))
		{
			i+=10;
			while(pszOutBuffer[i] != '<')
			{
				metar[a] = pszOutBuffer[i];
				a++;
				i++;
			}
		}
	}

	WrapMetar35(metar);

	if(metar[0] == 0x00)
	{
		sprintf(gMETAR, "Station Unavailable");
	}

	hr = SimConnect_WeatherSetObservation(hSimConnect,
		0,
		metar);

	delete[] pszOutBuffer;
}

//
// Set float properties
//
bool GaugeCallback::SetPropertyValue (SINT32 id, FLOAT64 value)
{
    GAUGE_VAR eGaugeVar = (GAUGE_VAR)id;

    switch(eGaugeVar)
    {
	    case GAUGE_VAR_ICAO1:
		{
			ICAO1 = GaugeCallback::CheckICAORange(ICAO1, value);

	        break;
		}

	    case GAUGE_VAR_ICAO2:
		{
			ICAO2 = GaugeCallback::CheckICAORange(ICAO2, value);

			break;
		}

	    case GAUGE_VAR_ICAO3:
		{
			ICAO3 = GaugeCallback::CheckICAORange(ICAO3, value);

			break;
		}

	    case GAUGE_VAR_ICAO4:
		{
			ICAO4 = GaugeCallback::CheckICAORange(ICAO4, value);

			break;
		}

		default:
		{
			return false;
		}
    }
    
	return true;
}


//
// String properties not permitted to be set
//
bool GaugeCallback::SetPropertyValue (SINT32 id, PCSTRINGZ szValue)
{
    return false;
}

// No implementation of this necessary
IGaugeCDrawable* GaugeCallback::CreateGaugeCDrawable(SINT32 id, const IGaugeCDrawableCreateParameters* pParameters)
{
    return NULL;
}

//-----------------------------------------------------------------------------------------------------------------
//
// AircraftCallback Override
//
//-----------------------------------------------------------------------------------------------------------------
class CABINAircraftCallback : public AircraftCallback
{

public:

	CABINAircraftCallback(UINT32 ContainerID): AircraftCallback(ContainerID)
	{
	}
    
	IGaugeCCallback* CreateGaugeCCallback ()
    {
        return new GaugeCallback(GetContainerId());
    }

};

//-----------------------------------------------------------------------------------------------------------------
//
// PanelCallback Override
//
//-----------------------------------------------------------------------------------------------------------------
class CABINPanelCallback : public PanelCallback
{

public:

	CABINPanelCallback::CABINPanelCallback()
	{
		// init property table
		for (int n = 0; n < totalVars; n++)
		{
			if ((ImportTable.PANELSentry.fnptr != NULL) && (CABIN_PROPERTY_TABLE[n].units == UNITS_UNKNOWN))
            {
                CABIN_PROPERTY_TABLE[n].units = get_units_enum (CABIN_PROPERTY_TABLE[n].szUnitsName);
            }
		}
	}
	
	IAircraftCCallback* CreateAircraftCCallback (UINT32 ContainerID)
    {
        return new CABINAircraftCallback(ContainerID);
    }

protected:

    const PROPERTY_TABLE *GetPropertyTable(UINT &uLength)
    {
        uLength = LENGTHOF(CABIN_PROPERTY_TABLE);
        return CABIN_PROPERTY_TABLE;
    }
};

void GaugeCallbackInit()
{
	CABINPanelCallback *pPanelCallback = new CABINPanelCallback();

    if (pPanelCallback)
    {
        bool b = panel_register_c_callback(callbackName, pPanelCallback);
        pPanelCallback->Release();
    }   
}

void GaugeCallbackDeInit()
{
    panel_register_c_callback(callbackName, NULL);
}

//
// If this DLL is listed in DLL.XML the panels pointer will get filled during the loading process
//
PPANELS Panels = NULL;

GAUGESIMPORT    ImportTable =                           
{                                                       
	{ 0x0000000F, (PPANELS)NULL },                     
    { 0x00000000, NULL }                                
};                                                      
   
void FSAPI  dllStart(void)
{
    if (NULL != Panels)
    {
        ImportTable.PANELSentry.fnptr = (PPANELS)Panels;
	    GaugeCallbackInit();
		InitializeSimConnect();
    }
}                         

void FSAPI  dllStop(void)
{
	GaugeCallbackDeInit();
	SimConnect_Close(hSimConnect);
}                       

BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved) 
{                                                       
    return TRUE;                                        
}                  
 
// This is the module's export table.
GAUGESLINKAGE   Linkage =                               
{                                                       
    0x00000013,                                         
    dllStart,                                       
    dllStop,                                      
    0,                                                  
    0,                        
    FS9LINK_VERSION, { 0 }
};