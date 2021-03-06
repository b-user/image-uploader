/*
    Image Uploader - program for uploading images/files to Internet
    Copyright (C) 2007-2015 ZendeN <zenden2k@gmail.com>

    HomePage:    http://zenden.ws/imageuploader

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This module contains uploading engine which uses SqPlus binding library
 for executing Squirrel scripts */

#include "ScriptUploadEngine.h"
#include <stdarg.h>
#include <iostream>

#include <openssl/md5.h>
#include <sqplus.h>
#include <sqstdsystem.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <json/json.h>
#include "Core/3rdpart/CP_RSA.h"
#include "Core/3rdpart/base64.h"
#include "Core/3rdpart/codepages.h"

#include "Core/Utils/CryptoUtils.h"
#ifndef IU_CLI
#include <Func/LangClass.h>
#include "Gui/Dialogs/LogWindow.h"
#endif
#include <Core/Upload/FileUploadTask.h>
#include <Core/Upload/UrlShorteningTask.h>
#include <sstream>
#include <Core/Utils/SimpleXml.h>
#include <Core/Utils/StringUtils.h>
#include <Core/Logging.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <Core/ScriptAPI/ScriptAPI.h>

using namespace SqPlus;
// Squirrel types should be defined in the same module where they are used
// otherwise we will catch SqPlus exception while executing Squirrel functions
///DECLARE_INSTANCE_TYPE(std::string);
using namespace  ScriptAPI;
DECLARE_INSTANCE_TYPE(ServerSettingsStruct);
DECLARE_INSTANCE_TYPE(NetworkClient);
DECLARE_INSTANCE_TYPE(CFolderList);
DECLARE_INSTANCE_TYPE(CFolderItem);
DECLARE_INSTANCE_TYPE(CIUUploadParams);
DECLARE_INSTANCE_TYPE(SimpleXml);
DECLARE_INSTANCE_TYPE(SimpleXmlNode);


#ifdef _WIN32
#ifndef IU_CLI
#include <Gui/Dialogs/InputDialog.h>
#include <Func/Common.h>
#include <Func/IuCommonFunctions.h>
const std::string Impl_AskUserCaptcha(NetworkClient *nm, const std::string& url)
{
	CString wFileName = GetUniqFileName(IuCommonFunctions::IUTempFolder+Utf8ToWstring("captcha").c_str());

	nm->setOutputFile(IuCoreUtils::WstringToUtf8((const TCHAR*)wFileName));
	if(!nm->doGet(url))
		return "";
	CInputDialog dlg(_T("Image Uploader"), TR("������� ����� � ��������:"), CString(IuCoreUtils::Utf8ToWstring("").c_str()),wFileName);
	nm->setOutputFile("");
	if(dlg.DoModal()==IDOK)
		return IuCoreUtils::WstringToUtf8((const TCHAR*)dlg.getValue());
	return "";
}
#endif
#endif

bool ShellOpenUrl(const std::string& url) {
#ifdef _WIN32
    return ShellExecute(0, _T("open"), IuCoreUtils::Utf8ToWstring(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
#ifdef __APPLE__
    return system(("open \""+url+"\"").c_str());
#else
    return system(("xdg-open \""+url+"\" >/dev/null 2>&1 & ").c_str());
#endif
#endif
}

const std::string AskUserCaptcha(NetworkClient* nm, const std::string& url)
{
#ifndef IU_CLI
	return Impl_AskUserCaptcha(nm, url);
#else
	ShellOpenUrl(url);
		std::cerr << "Enter text from the image:"<<std::endl;
#ifdef _WIN32
		std::wstring result;
		std::wcin>>result;
		return IuCoreUtils::WstringToUtf8(result);
#else
		std::string result;
		std::cin>>result;
		return result;
#endif
		
	
#endif
	return "";
}



const std::string InputDialog(const std::string& text, const std::string& defaultValue)
{
#ifndef IU_CLI
	return Impl_InputDialog(text, defaultValue);
#else
	std::string result;
	std::cerr<<std::endl<<text<<std::endl;
	std::cin>>result;
	return result;
#endif
	return "";
}


void CFolderList::AddFolder(const std::string& title, const std::string& summary, const std::string& id,
                            const std::string& parentid,
                            int accessType)
{
	CFolderItem ai;
	ai.title = (title);
	ai.summary = (summary);
	ai.id = (id);
	ai.parentid =  (parentid);
	ai.accessType = accessType;
	m_folderItems.push_back(ai);
}

void CFolderList::AddFolderItem(const CFolderItem& item)
{
	m_folderItems.push_back(item);
}

// older versions of Squirrel Standart Library have broken srand() function
int pluginRandom()
{
	return rand();
}

std::string squirrelOutput;
const Utf8String IuNewFolderMark = "_iu_create_folder_";
static void printFunc(HSQUIRRELVM v, const SQChar* s, ...)
{
	va_list vl;
	va_start(vl, s);
	int len = 1024; // _vcsprintf( s,vl ) + 1;
	char* buffer = new char [len + 1];
	vsnprintf( buffer, len, s, vl);
	va_end(vl);
	// std::wstring text =  Utf8ToWstring(buffer);
	squirrelOutput += buffer;
	delete[] buffer; 
}

void CompilerErrorHandler(HSQUIRRELVM,const SQChar * desc,const SQChar * source,SQInteger line,SQInteger column) {
	LOG(ERROR) << "Script compilation failed\r\n"<<"File:  "<<source<<"\r\nLine:"<<line<<"   Column:"<<column<<"\r\n\r\n"<<desc;
}


void CScriptUploadEngine::InitScriptEngine()
{
	SquirrelVM::Init();
	sq_setcompilererrorhandler(SquirrelVM::GetVMPtr(), CompilerErrorHandler);
	SquirrelVM::PushRootTable();
	sqstd_register_systemlib( SquirrelVM::GetVMPtr() );
}

void CScriptUploadEngine::DestroyScriptEngine()
{
	SquirrelVM::Shutdown();
}

void CScriptUploadEngine::FlushSquirrelOutput()
{
	if (!squirrelOutput.empty())
	{
		Log(ErrorInfo::mtWarning, "Squirrel\r\n" + /*IuStringUtils::ConvertUnixLineEndingsToWindows*/(squirrelOutput));
		squirrelOutput.clear();
	}
}

int CScriptUploadEngine::doUpload(UploadTask* task, CIUUploadParams &params)
{
	std::string FileName;

	if ( task->getType() == "file" ) {
		FileName = ((FileUploadTask*)task)->getFileName();
	}
	CFolderItem parent, newFolder = m_ServersSettings.newFolder;
	std::string folderID = m_ServersSettings.params["FolderID"];

	if (folderID == IuNewFolderMark)
	{
		SetStatus(stCreatingFolder, newFolder.title);
		if ( createFolder(parent, newFolder))
		{
			folderID = newFolder.id;
			m_ServersSettings.params["FolderID"] = folderID;
			m_ServersSettings.params["FolderUrl"] = newFolder.viewUrl;
		}
		else
			folderID.clear();
	}

	params.folderId = folderID;

	int ival = 0;
	try
	{
		SetStatus(stUploading);
		if ( task->getType() == "file" ) {
			SquirrelFunction<int> func(m_Object, _SC("UploadFile"));
			std::string fname = FileName;
			ival = func(fname.c_str(), &params); // Argument coun*/
		} else if ( task->getType() == "url" ) {
			UrlShorteningTask *urlShorteningTask = static_cast<UrlShorteningTask*>(task);
			SquirrelFunction<int> func(m_Object, _SC("ShortenUrl"));
			std::string url = urlShorteningTask->getUrl();
			ival = func(url.c_str(), &params) && !params.DirectUrl.empty(); // Argument coun*/
		}
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::uploadFile\r\n" + Utf8String(e.desc));
	}
	FlushSquirrelOutput();
	return ival != 0;
}

const std::string plugExtractFileName(const std::string& path)
{
	std::string res = IuCoreUtils::ExtractFileName(path);
	return res;
}

const std::string plugGetFileExtension(const std::string& path)
{
	std::string res = IuCoreUtils::ExtractFileExt(path);
	return res;
}

bool CScriptUploadEngine::needStop()
{
	bool m_bShouldStop = false;
	if (onNeedStop)
		m_bShouldStop = onNeedStop();  // delegate call
	return m_bShouldStop;
}

CScriptUploadEngine::CScriptUploadEngine(Utf8String pluginName) : CAbstractUploadEngine()
{
	m_sName = pluginName;
	m_CreationTime = time(0);
}

CScriptUploadEngine::~CScriptUploadEngine()
{
	// SquirrelVM::Shutdown();
}

const std::string YandexRsaEncrypter(const std::string& key, const std::string& data)
{
	if (key.empty())
		return "";              // otherwise we get segfault
	// std::cout<<"key="<<key<<"  data="<<data<<std::endl;
	CCryptoProviderRSA encrypter;
	encrypter.ImportPublicKey(key.c_str());
	char crypted_data[MAX_CRYPT_BITS / sizeof(char)] = "\0";
	size_t crypted_len = 0;
	encrypter.Encrypt(data.c_str(), data.length(), crypted_data, crypted_len);

	return base64_encode((unsigned char*)crypted_data, crypted_len);
}

const std::string scriptGetFileMimeType(const std::string& filename)
{
	return IuCoreUtils::GetFileMimeType(filename);
}

const std::string scriptAnsiToUtf8(const std::string& str, int codepage)
{
#ifdef _WIN32
	return IuCoreUtils::ConvertToUtf8(str, NameByCodepage(codepage));
#else
	LOG(WARNING) << "AnsiToUtf8 not implemented";
	return str; // FIXME
#endif
}

const std::string scriptUtf8ToAnsi(const std::string& str, int codepage )
{
#ifdef _WIN32
	return IuCoreUtils::Utf8ToAnsi(str, codepage);
#else
	LOG(WARNING) << "Utf8ToAnsi not implemented";
	return str; // FIXME
#endif
}

void scriptWriteLog(const std::string& type, const std::string& message) {
#ifndef IU_CLI
	LogMsgType msgType = logWarning;
	if ( type == "error" ) {
		msgType = logError;
	}
	WriteLog(msgType,_T("Script Engine"),Utf8ToWCstring(message));
#else
	std::cerr << type <<" : ";
	#ifdef _WIN32
		std::wcerr<<IuCoreUtils::Utf8ToWstring(message)<<std::endl;;
	#else
		std::cerr<<IuCoreUtils::Utf8ToSystemLocale(message)<<std::endl;
	#endif
#endif
}

const std::string scriptMD5(const std::string& data)
{
	return IuCoreUtils::CryptoUtils::CalcMD5HashFromString(data);
}

void scriptSleep(int msec) {
#ifdef _WIN32
	Sleep(msec);
#else
    sleep(ceil(msec/1000.0));
#endif
}

/*bool ShowText(const std::string& data) {
	return DebugMessage( data, true );
}*/

const std::string escapeJsonString( const std::string& src) {
	return Json::valueToQuotedString(src.data());
}

const std::string scriptGetTempDirectory() {
#ifdef _WIN32
	#ifndef IU_CLI
		return IuCoreUtils::WstringToUtf8((LPCTSTR)IuCommonFunctions::IUTempFolder);
	#else
	TCHAR ShortPath[1024];
	GetTempPath(ARRAY_SIZE(ShortPath), ShortPath);
	TCHAR TempPath[1024];
	if (!GetLongPathName(ShortPath,TempPath, ARRAY_SIZE(TempPath)) ) {
		lstrcpy(TempPath, ShortPath);
	}
	return IuCoreUtils::WstringToUtf8(TempPath);
	#endif
#else
	return "/var/tmp/";
#endif
}

const std::string url_encode(const std::string &value) {
	using namespace std;
	ostringstream escaped;
	escaped.fill('0');
	escaped << hex << std::uppercase;

	for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << '%' << setw(2) << int((unsigned char) c);
	}

	return escaped.str();
}


void DebugMessage(const std::string& msg, bool isResponseBody)
{
#ifndef IU_CLI
	DefaultErrorHandling::DebugMessage(msg,isResponseBody);
#else
#ifdef _WIN32
	std::wcerr<<IuCoreUtils::Utf8ToWstring(msg)<<std::endl;;
#else
	std::cerr<<IuCoreUtils::Utf8ToSystemLocale(msg)<<std::endl;
#endif
    getc(stdin);
#endif
}

const std::string scriptMessageBox( const std::string& message, const std::string &title,const std::string& buttons , const std::string& type) {
#if defined(_WIN32) && !defined(IU_CLI)
	UINT uButtons = MB_OK;
	if ( buttons == "ABORT_RETRY_IGNORE") {
		uButtons = MB_ABORTRETRYIGNORE;
	} else if ( buttons == "CANCEL_TRY_CONTINUE") {
		uButtons = MB_CANCELTRYCONTINUE;
	} else if ( buttons == "OK_CANCEL") {
		uButtons = MB_OKCANCEL;
	} else if ( buttons == "RETRY_CANCEL") {
		uButtons = MB_RETRYCANCEL;
	} else if ( buttons == "YES_NO") {
		uButtons = MB_YESNO;
	}else if ( buttons == "YES_NO_CANCEL") {
		uButtons = MB_YESNOCANCEL;
	}
	UINT icon = 0;
	if ( type == "EXCLAMATION") {
		icon = MB_ICONEXCLAMATION;
	} else if ( type == "WARNING") {
		icon = MB_ICONWARNING;
	} else if ( type == "INFORMATION") {
		icon = MB_ICONINFORMATION;
	} else if ( type == "QUESTION") {
		icon = MB_ICONQUESTION;
	} else if ( type == "ERROR") {
		icon = MB_ICONERROR;
	} 
	int res = MessageBox(GetActiveWindow(), IuCoreUtils::Utf8ToWstring(message).c_str(),  IuCoreUtils::Utf8ToWstring(title).c_str(), uButtons |icon );
	if ( res == IDABORT ) {
		return "ABORT";
	} else if ( res == IDCANCEL ) {
		return "CANCEL";
	} else if ( res == IDCONTINUE ) {
		return "CONTINUE";
	} else if ( res == IDIGNORE ) {
		return "IGNORE";
	} else if ( res == IDNO ) {
		return "NO";
	} else if ( res == IDOK ) {
		return "OK";
	} else if ( res == IDYES ) {
		return "YES";
	}
	else if ( res == IDRETRY ) {
		return "TRY";
	} else if ( res == IDTRYAGAIN ) {
		return "TRY";
	} 
	return "";
	
#else
	std::cerr<<"----------";
#ifdef _WIN32
	std::wcerr<<IuCoreUtils::Utf8ToWstring(title);
#else
	std::cerr<<IuCoreUtils::Utf8ToSystemLocale(title);
#endif
	std::cerr<<"----------"<<std::endl;
#ifdef _WIN32
	std::wcerr<<IuCoreUtils::Utf8ToWstring(message)<<std::endl;;
#else
	std::cerr<<IuCoreUtils::Utf8ToSystemLocale(message)<<std::endl;;
#endif
	if ( buttons.empty() || buttons == "OK") {
		    getc(stdin);
			return "OK";
	} else {
		

		std::vector<std::string> tokens;
		std::map<char,std::string> buttonsMap;
		IuStringUtils::Split(buttons,"_", tokens,10);
		for(int i = 0; i< tokens.size(); i++ ) {
			if ( i !=0 ) {
				std::cerr<< "/";
			}
			buttonsMap[tokens[i][0]] = tokens[i];
			std::cerr<< "("<<tokens[i][0]<<")"<<IuStringUtils::toLower(tokens[i]).c_str()+1;
		}
		std::cerr<<": ";
		char res;
		std::cin >> res;
		res = toupper(res);
		return buttonsMap[res];
	}

#endif
}

template<class T> void setObjValues(T key, Json::ValueIterator it, SquirrelObject &obj) {
	using namespace Json;

	switch(it->type()) {
		case nullValue:
			obj.SetValue(key,SquirrelObject());
			break;
		case intValue:      ///< signed integer value
			obj.SetValue(key, it->asInt());
			break;
		case uintValue:     ///< unsigned integer value
			obj.SetValue(key, it->asInt());
			break;
		case realValue:
			obj.SetValue(key, it->asFloat());
			break;    ///< double value
		case stringValue:   ///< UTF-8 string value
			obj.SetValue(key, it->asString().data());
			break;   
		case booleanValue:  ///< bool value
			obj.SetValue(key, it->asBool());
			break;   
		case arrayValue:    ///< array value (ordered list)
		case objectValue:
			SquirrelObject newObj;
			obj.SetValue(key, parseJSONObj(*it,newObj));
	}
}

SquirrelObject parseJSONObj(Json::Value root, SquirrelObject &obj) {
	Json::ValueIterator it;
	//SquirrelObject obj;
	bool isArray = root.isArray();
	if ( isArray ) {
		obj = SquirrelVM::CreateArray(root.size());
	} else if ( root.isObject() ) {
		obj = SquirrelVM::CreateTable();
	} 
	

	if ( isArray ) {
		for(it = root.begin(); it != root.end(); ++it) {
			int key = it.key().asInt();
			
			setObjValues(key, it, obj);
		}
	} else {
		for(it = root.begin(); it != root.end(); ++it) {
			std::string key = it.key().asString();
			setObjValues(key.data(), it, obj);
		}
	}
	
	return obj;
}

SquirrelObject jsonToSquirrelObject(const std::string& json) {
	Json::Value root;
	Json::Reader reader;
	SquirrelObject sq;
	if ( reader.parse(json, root, false) ) {
		parseJSONObj(root,sq);
	}
	return sq;
}

Json::Value sqValueToJson(SquirrelObject obj ) {
	switch ( obj.GetType() ) {
		case OT_NULL:
			return Json::Value(Json::nullValue);
		case OT_INTEGER:
			return obj.ToInteger();
			break;
		case OT_FLOAT:
			return obj.ToFloat();
			break;

		case OT_BOOL:
			return obj.ToBool();
			break;

		case OT_STRING:
			return obj.ToString();
			break;
	}
	return Json::Value(Json::nullValue);
}
Json::Value sqObjToJson(	SquirrelObject obj ) {
	Json::Value res;
	SquirrelObject key;
	SquirrelObject value;
	switch ( obj.GetType() ) {
			case OT_NULL:
			case OT_INTEGER:
			case OT_FLOAT:
			case OT_BOOL:
			case OT_STRING:
				return sqValueToJson(obj);
				break;
			case OT_TABLE:
				if ( obj.BeginIteration() ) {
					while(obj.Next(key,value) ) {
						res[key.ToString()]=sqObjToJson(value);
					}
					obj.EndIteration();
				}
				return res;
				break;
			case OT_ARRAY: 
				if ( obj.BeginIteration() ) {
					while(obj.Next(key,value) ) {
						res[key.ToInteger()]=sqObjToJson(value);
					}
					obj.EndIteration();

				}
				return res;
				break;				
	}
	return Json::Value(Json::nullValue);
}

const std::string squirrelObjectToJson(SquirrelObject  obj) {
	Json::Value root = sqObjToJson(obj);
	Json::StreamWriterBuilder builder;
	builder["commentStyle"] = "None";
	builder["indentation"] = "   ";  // or whatever you like

	return Json::writeString(builder, root);
}


const std::string GetFileContents(const std::string& filename) {
	std::string data;
	IuCoreUtils::ReadUtf8TextFile(filename, data);
	return data;
}

unsigned int ScriptGetFileSize(const std::string& filename) {
	return IuCoreUtils::getFileSize(filename);
}

double ScriptGetFileSizeDouble(const std::string& filename) {
	return IuCoreUtils::getFileSize(filename);
}

const std::string scriptGetAppLanguage() {
#ifndef IU_CLI
	return IuCoreUtils::WstringToUtf8((LPCTSTR)Lang.getLanguage());
#else 
	return "en";
#endif 
}

const std::string scriptGetAppLocale() {
#ifndef IU_CLI
	return IuCoreUtils::WstringToUtf8((LPCTSTR)Lang.getLocale());
#else 
	return "en_US";
#endif 
}
bool CScriptUploadEngine::load(Utf8String fileName, ServerSettingsStruct& params)
{
	if (!IuCoreUtils::FileExists(fileName))
		return false;

	using namespace ScriptAPI;
	setServerSettings(params);
	try
	{
		m_Object = SquirrelVM::CreateTable();

		sq_setprintfunc(SquirrelVM::GetVMPtr(), printFunc /*,printFunc*/);

		SQClassDef<NetworkClient>("NetworkClient").
		func(&NetworkClient::doGet, "doGet").
		func(&NetworkClient::responseBody, "responseBody").
		func(&NetworkClient::responseCode, "responseCode").
		func(&NetworkClient::setUrl, "setUrl").
		func(&NetworkClient::doPost, "doPost").
		func(&NetworkClient::addQueryHeader, "addQueryHeader").
		func(&NetworkClient::addQueryParam, "addQueryParam").
		func(&NetworkClient::addQueryParamFile, "addQueryParamFile").
		func(&NetworkClient::responseHeaderCount, "responseHeaderCount").
		func(&NetworkClient::urlEncode, "urlEncode").
		func(&NetworkClient::errorString, "errorString").
		func(&NetworkClient::doUpload, "doUpload").
		func(&NetworkClient::setMethod, "setMethod").
		func(&NetworkClient::setCurlOption, "setCurlOption").
		func(&NetworkClient::setCurlOptionInt, "setCurlOptionInt").
		func(&NetworkClient::doUploadMultipartData, "doUploadMultipartData").
		func(&NetworkClient::enableResponseCodeChecking, "enableResponseCodeChecking").
		func(&NetworkClient::setChunkSize, "setChunkSize").
		func(&NetworkClient::setChunkOffset, "setChunkOffset").
		func(&NetworkClient::setUserAgent, "setUserAgent").
		func(&NetworkClient::responseHeaderText, "responseHeaderText").
		func(&NetworkClient::responseHeaderByName, "responseHeaderByName").
		func(&NetworkClient::setReferer, "setReferer");


		SQClassDef<CFolderList>("CFolderList").
		func(&CFolderList::AddFolder, "AddFolder").
		func(&CFolderList::AddFolderItem, "AddFolderItem");

		SQClassDef<ServerSettingsStruct>("ServerSettingsStruct").
		func(&ServerSettingsStruct::setParam, "setParam").
		func(&ServerSettingsStruct::getParam, "getParam");

		SQClassDef<CIUUploadParams>("CIUUploadParams").
		func(&CIUUploadParams::getFolderID, "getFolderID").
		func(&CIUUploadParams::setDirectUrl, "setDirectUrl").
		func(&CIUUploadParams::setThumbUrl, "setThumbUrl").
		func(&CIUUploadParams::getServerFileName, "getServerFileName").
		func(&CIUUploadParams::setViewUrl, "setViewUrl").
		func(&CIUUploadParams::getParam, "getParam");

		SQClassDef<CFolderItem>("CFolderItem").
		func(&CFolderItem::getId, "getId").
		func(&CFolderItem::getParentId, "getParentId").
		func(&CFolderItem::getSummary, "getSummary").
		func(&CFolderItem::getTitle, "getTitle").
		func(&CFolderItem::setId, "setId").
		func(&CFolderItem::setParentId, "setParentId").
		func(&CFolderItem::setSummary, "setSummary").
		func(&CFolderItem::setTitle, "setTitle").
		func(&CFolderItem::getAccessType, "getAccessType").
		func(&CFolderItem::setAccessType, "setAccessType").
		func(&CFolderItem::setItemCount, "setItemCount").
		func(&CFolderItem::setViewUrl, "setViewUrl").
		func(&CFolderItem::getItemCount, "getItemCount");


		SQClassDef<SimpleXml>("SimpleXml").
			func(&SimpleXml::LoadFromFile, "LoadFromFile").
			func(&SimpleXml::LoadFromString, "LoadFromString").
			func(&SimpleXml::SaveToFile, "SaveToFile").
			func(&SimpleXml::ToString, "ToString").
			func(&SimpleXml::getRoot, "GetRoot");

		SQClassDef<SimpleXmlNode>("SimpleXmlNode").
			func(&SimpleXmlNode::Attribute, "Attribute").
			func(&SimpleXmlNode::AttributeInt, "AttributeInt").
		    func(&SimpleXmlNode::AttributeBool, "AttributeBool").
			func(&SimpleXmlNode::Name, "Name").
			func(&SimpleXmlNode::Text, "Text").
			func(&SimpleXmlNode::CreateChild, "CreateChild").
			func(&SimpleXmlNode::GetChild, "GetChild").
			func(&SimpleXmlNode::SetAttributeString, "SetAttribute").
			func(&SimpleXmlNode::SetAttributeInt, "SetAttributeInt").
			func(&SimpleXmlNode::SetAttributeBool, "SetAttributeBool").
			func(&SimpleXmlNode::SetText, "SetText").
			func(&SimpleXmlNode::IsNull, "IsNull").
			func(&SimpleXmlNode::DeleteChilds, "DeleteChilds").
			func(&SimpleXmlNode::GetChildCount, "GetChildCount").
			func(&SimpleXmlNode::GetChildByIndex, "GetChildByIndex").
			func(&SimpleXmlNode::GetAttributeCount, "GetAttributeCount");
			
			/*func(&SimpleXmlNode::GetChild, "GetChild")*/;
		ScriptAPI::RegisterRegularExpressionClass();

		//using namespace IuCoreUtils;
		RegisterGlobal(pluginRandom, "random");
		RegisterGlobal(scriptSleep, "sleep");
		RegisterGlobal(scriptMD5, "md5");
		RegisterGlobal(scriptAnsiToUtf8, "AnsiToUtf8");
		RegisterGlobal(YandexRsaEncrypter, "YandexRsaEncrypter");
		RegisterGlobal(scriptUtf8ToAnsi, "Utf8ToAnsi");
		RegisterGlobal(plugExtractFileName, "ExtractFileName");
		RegisterGlobal(plugGetFileExtension, "GetFileExtension");
		RegisterGlobal(AskUserCaptcha, "AskUserCaptcha");
		RegisterGlobal(InputDialog, "InputDialog");
		RegisterGlobal(scriptGetFileMimeType, "GetFileMimeType");
		RegisterGlobal(escapeJsonString, "JsonEscapeString");
		RegisterGlobal(ShellOpenUrl, "ShellOpenUrl");
		RegisterGlobal(IuCoreUtils::ExtractFileNameNoExt, "ExtractFileNameNoExt");
		RegisterGlobal(IuCoreUtils::ExtractFilePath, "ExtractFilePath");
		RegisterGlobal(jsonToSquirrelObject, "ParseJSON");
		RegisterGlobal(squirrelObjectToJson, "ToJSON");
		RegisterGlobal(IuCoreUtils::copyFile, "CopyFile");
		RegisterGlobal(IuCoreUtils::createDirectory, "CreateDirectory");
		RegisterGlobal(IuCoreUtils::FileExists, "FileExists");
		RegisterGlobal(GetFileContents, "GetFileContents");
		RegisterGlobal(scriptGetTempDirectory, "GetTempDirectory");
		RegisterGlobal(IuCoreUtils::MoveFileOrFolder, "MoveFileOrFolder");
		RegisterGlobal(IuCoreUtils::PutFileContents, "PutFileContents");
		RegisterGlobal(IuCoreUtils::RemoveFile, "DeleteFile");
		RegisterGlobal(scriptGetAppLanguage, "GetAppLanguage");
		RegisterGlobal(scriptGetAppLocale, "GetAppLocale");
		
		RegisterGlobal(ScriptGetFileSize, "GetFileSize");	
		RegisterGlobal(ScriptGetFileSizeDouble, "GetFileSizeDouble");	
		RegisterGlobal(scriptWriteLog, "WriteLog");	
		RegisterGlobal(scriptMessageBox, "MessageBox");	

		using namespace IuCoreUtils;
		RegisterGlobal(&CryptoUtils::CalcMD5HashFromFile, "md5_file");
		RegisterGlobal(&CryptoUtils::CalcSHA1HashFromString, "sha1");
		RegisterGlobal(&CryptoUtils::CalcSHA1HashFromFile, "sha1_file");
		RegisterGlobal(&CryptoUtils::CalcHMACSHA1HashFromString, "hmac_sha1");
		RegisterGlobal(url_encode, "url_encode");
		
		srand(static_cast<unsigned int>(time(0)));

		ScriptAPI::RegisterFunctions(&m_Object);
		ScriptAPI::RegisterClasses(&m_Object);

		RegisterGlobal(::DebugMessage, "DebugMessage" );

		BindVariable(m_Object, &params, "ServerParams");

		std::string scriptText;
		IuCoreUtils::ReadUtf8TextFile(fileName, scriptText);
		m_SquirrelScript = SquirrelVM::CompileBuffer(scriptText.c_str(), IuCoreUtils::ExtractFileName(fileName).c_str());
		SquirrelVM::RunScript(m_SquirrelScript, &m_Object);
		ScriptAPI::RegisterShortTranslateFunctions(!m_Object.Exists("tr"), !m_Object.Exists("__"));
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::Load failed\r\n" + std::string("Error: ") + e.desc);
		return false;
	}
	FlushSquirrelOutput();
	return true;
}

int CScriptUploadEngine::getAccessTypeList(std::vector<Utf8String>& list)
{
	try
	{
		SquirrelFunction<SquirrelObject> func(m_Object, _SC("GetFolderAccessTypeList"));
		if (func.func.IsNull())
			return -1;
		SquirrelObject arr = func();

		list.clear();
		int count =  arr.Len();
		for (int i = 0; i < count; i++)
		{
			std::string title;
			title = arr.GetString(i);
			list.push_back(title);
		}
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::getAccessTypeList\r\n" + std::string(e.desc));
	}
	FlushSquirrelOutput();
	return 1;
}

int CScriptUploadEngine::getServerParamList(std::map<Utf8String, Utf8String>& list)
{
	SquirrelObject arr;

	try
	{
		SquirrelFunction<SquirrelObject> func(m_Object, _SC("GetServerParamList"));
		if (func.func.IsNull())
			return -1;
		arr = func();

		list.clear();
		SquirrelObject key, value;
		arr.BeginIteration();
		while (arr.Next(key, value))
		{
			const SQChar* t = value.ToString();
			if ( t ) {
				Utf8String title = t;
				list[key.ToString()] =  title;
			}
		}
		arr.EndIteration();
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::getServerParamList\r\n" + std::string(e.desc));
	}
	FlushSquirrelOutput();
	return 1;
}

int CScriptUploadEngine::doLogin()
{
	SquirrelObject arr;

	try
	{
		SquirrelFunction<int> func(m_Object, _SC("DoLogin"));
		if (func.func.IsNull())
			return 0;
		return func();
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::doLogin\r\n" + std::string(e.desc));
		return 0;
	}
	FlushSquirrelOutput();
}

int CScriptUploadEngine::modifyFolder(CFolderItem& folder)
{
	try
	{
		SquirrelFunction<int> func(m_Object, _SC("ModifyFolder"));
		if (func.func.IsNull())
			return -1;
		func(&folder);
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::getServerParamList\r\n" + Utf8String(e.desc));
	}
	FlushSquirrelOutput();
	return 1;
}

int CScriptUploadEngine::getFolderList(CFolderList& FolderList)
{
	int ival = 0;
	try
	{
		SquirrelFunction<int> func(m_Object, _SC("GetFolderList"));
		if (func.func.IsNull())
			return -1;
		ival = func(&FolderList);
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::getFolderList\r\n" + Utf8String(e.desc));
	}
	FlushSquirrelOutput();
	return ival;
}

bool CScriptUploadEngine::isLoaded()
{
	return m_bIsPluginLoaded;
}

Utf8String CScriptUploadEngine::name()
{
	return m_sName;
}

int CScriptUploadEngine::createFolder(CFolderItem& parent, CFolderItem& folder)
{
	int ival = 0;
	try
	{
		SquirrelFunction<int> func(m_Object, _SC("CreateFolder"));
		if (func.func.IsNull())
			return -1;
		ival = func(&parent, &folder);
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::createFolder\r\n" + Utf8String(e.desc));
	}
	FlushSquirrelOutput();
	return ival;
}

time_t CScriptUploadEngine::getCreationTime()
{
	return m_CreationTime;
}

void CScriptUploadEngine::setNetworkClient(NetworkClient* nm)
{
	CAbstractUploadEngine::setNetworkClient(nm);
	BindVariable(m_Object, nm, "nm");
}

bool CScriptUploadEngine::supportsSettings()
{
	SquirrelObject arr;

	try
	{
		SquirrelFunction<SquirrelObject> func(m_Object, _SC("GetServerParamList"));
		if (func.func.IsNull())
			return false;
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::supportsSettings\r\n" + std::string(e.desc));
		return false;
	}
	FlushSquirrelOutput();
	return true;
}

bool CScriptUploadEngine::supportsBeforehandAuthorization()
{
	SquirrelObject arr;

	try
	{
		SquirrelFunction<SquirrelObject> func(m_Object, _SC("DoLogin"));
		if (func.func.IsNull())
			return false;
	}
	catch (SquirrelError& e)
	{
		Log(ErrorInfo::mtError, "CScriptUploadEngine::supportsBeforehandAuthorization\r\n" + std::string(e.desc));
		return false;
	}
	FlushSquirrelOutput();
	return true;
}

int CScriptUploadEngine::RetryLimit()
{
	return m_UploadData->RetryLimit;
}

void CScriptUploadEngine::Log(ErrorInfo::MessageType mt, const std::string& error)
{
	ErrorInfo ei;
	ei.ActionIndex = -1;
	ei.messageType = mt;
	ei.errorType = etUserError;
	ei.error = error;
	ei.sender = "CScriptUploadEngine";
	ErrorMessage(ei);
}

