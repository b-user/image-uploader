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

#include "UploadEngineList.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include "Core/Upload/DefaultUploadEngine.h"
#include "Core/Utils/SimpleXml.h"
#include "Core/Utils/StringUtils.h"
#include <Core/Logging.h>
#include "versioninfo.h"

bool compareEngines(const CUploadEngineData& elem1, const CUploadEngineData& elem2)
{
	return IuStringUtils::stricmp(elem1.Name.c_str(), elem2.Name.c_str()) < 0;
}

CUploadEngineList::CUploadEngineList()
{
	m_EngineNumOfRetries = 3;
	m_ActionNumOfRetries = 2;
}

bool CUploadEngineList::LoadFromFile(const std::string& filename,std::map <std::string, std::map <std::string, ServerSettingsStruct>>& serversSettings)
{
	SimpleXml xml;
	if(!xml.LoadFromFile(filename))
		return false;
		
	SimpleXmlNode root = xml.getRoot("ServerListTool");

	std::vector<SimpleXmlNode> childs;
	root.GetChilds("Server", childs);
	root.GetChilds("Server2", childs);
	root.GetChilds("Server3", childs);

	std::string ver = _APP_VER;
	std::vector<std::string> tokens;
	IuStringUtils::Split(ver,".", tokens, 3);
	int majorVersion = (int)IuCoreUtils::stringToint64_t(tokens[0]);
	int minorVersion = (int)IuCoreUtils::stringToint64_t(tokens[1]+tokens[2]);
	int build = (int)IuCoreUtils::stringToint64_t(BUILD);

	for(size_t i=0; i<childs.size(); i++)
	{
		SimpleXmlNode &cur = childs[i];
		CUploadEngineData UE;
		UE.NumOfTries = 0;
		UE.NeedAuthorization = cur.AttributeInt("Authorize");
		std::string needPassword = cur.Attribute("NeedPassword");
		UE.NeedPassword = needPassword.empty() ? true : (bool)IuCoreUtils::stringToint64_t(needPassword);
		
		std::string RetryLimit = cur.Attribute("RetryLimit");
		if(RetryLimit.empty())
		{
			// load from settings ;;;
			UE.RetryLimit = m_EngineNumOfRetries;//Settings.FileRetryLimit;
		}
			else
				UE.RetryLimit = atoi(RetryLimit.c_str());

			UE.Name =  cur.Attribute("Name");

			std::string serverMinVersion = cur.Attribute("MinVersion");
			if ( !serverMinVersion.empty() ) {
				std::vector<std::string> tokens;
				IuStringUtils::Split(serverMinVersion,".", tokens, 4);
				if ( tokens.size() >= 3 ) {
					int serverMajorVersion = (int)IuCoreUtils::stringToint64_t(tokens[0]);
					int serverMinorVersion = (int)IuCoreUtils::stringToint64_t(tokens[1]+tokens[2]);
					int serverBuild = tokens.size() > 3 ? IuCoreUtils::stringToint64_t(tokens[3]) : 0;
					if ( !( majorVersion > serverMajorVersion || ( majorVersion == serverMajorVersion && minorVersion > serverMinorVersion) 
						|| ( majorVersion == serverMajorVersion && minorVersion ==  serverMinorVersion && ( !serverBuild || build >= serverBuild ))
						) ) {
							continue;
					}
				}
			}

			UE.SupportsFolders = cur.AttributeBool("SupportsFolders");
			UE.RegistrationUrl = cur.Attribute("RegistrationUrl");
			UE.PluginName = cur.Attribute("Plugin");
			if ( UE.PluginName == "ftp" ) {
				if ( serversSettings[UE.Name].size() ) {
					std::string hostname = serversSettings[UE.Name].begin()->second.getParam("hostname");
					if ( hostname.empty() || hostname == "ftp.example.com" ) {
						//LOG(WARNING) << "Skipping server  "<< UE.Name;
						continue;
					}
				} else {
					continue;
				}
			}
			UE.UsingPlugin = !UE.PluginName.empty();
			UE.Debug =   cur.AttributeBool("Debug");
			UE.ImageHost =  !cur.AttributeBool("FileHost");
			UE.MaxFileSize =   cur.AttributeInt("MaxFileSize");

			std::string typeString =  cur.Attribute("Type");
			UE.Type = UE.ImageHost ? CUploadEngineData::TypeImageServer : CUploadEngineData::TypeFileServer; 
			
			if ( !typeString.empty() ) {	
				if ( typeString == "image" ) {
					UE.Type = CUploadEngineData::TypeImageServer;
				} else if ( typeString == "file" ) {
					UE.Type =  CUploadEngineData::TypeFileServer;
				} else if ( typeString == "text" ) {
					UE.Type = CUploadEngineData::TypeTextServer;
				} else if ( typeString == "urlshortening") {
					UE.Type = CUploadEngineData::TypeUrlShorteningServer;
				} 
				UE.ImageHost = (UE.Type == CUploadEngineData::TypeImageServer);
			}

			std::vector<SimpleXmlNode> actions;
			cur["Actions"].GetChilds("Action", actions);

			for(size_t j=0; j<actions.size(); j++)
			{
				SimpleXmlNode &actionNode = actions[j];
				UploadAction UA;
				UA.NumOfTries = 0;
				UA.Index = j;

				std::string RetryLimit = actionNode.Attribute("RetryLimit");
				if(RetryLimit.empty())
				{
					UA.RetryLimit = m_ActionNumOfRetries;//Settings.ActionRetryLimit;
				}
				else UA.RetryLimit = atoi(RetryLimit.c_str());
				UA.IgnoreErrors = actionNode.AttributeBool("IgnoreErrors");
				UA.Description= actionNode.Attribute("Description");

				UA.Type = actionNode.Attribute("Type");
				UA.Url = actionNode.Attribute("Url");

				UA.PostParams = actionNode.Attribute("PostParams");
				UA.RegExp = actionNode.Attribute("RegExp");
				UA.OnlyOnce = actionNode.AttributeBool("OnlyOnce");

				std::string AssignVars = actionNode.Attribute("AssignVars");

				std::vector<std::string> Vars;
				nm_splitString(AssignVars, ";", Vars);

				for(std::vector<std::string>::iterator it = Vars.begin(); it!=Vars.end(); it++)
				{
					std::vector<std::string> NameAndValue;
				//	std::cout<<"*************"<<*it<<std::endl;
					nm_splitString(*it, ":", NameAndValue);
					if(NameAndValue.size() == 2)
					{
						ActionVariable AV;
						AV.Name = NameAndValue[0];
						AV.nIndex = atoi(NameAndValue[1].c_str());
						UA.Variables.push_back(AV);
					}
				}

				UE.Actions.push_back(UA);
			}

			SimpleXmlNode resultNode = cur["Result"];
			{

				UE.DownloadUrlTemplate = resultNode.Attribute("DownloadUrlTemplate");
				UE.ImageUrlTemplate = resultNode.Attribute("ImageUrlTemplate");
				UE.ThumbUrlTemplate = resultNode.Attribute("ThumbUrlTemplate");
				std::string directUrlTemplate = resultNode.Attribute("DirectUrlTemplate"); 
				if ( !directUrlTemplate.empty() ) {
					UE.ImageUrlTemplate = directUrlTemplate;
				}

			}
			UE.SupportThumbnails = !UE.ThumbUrlTemplate.empty();
			m_list.push_back(UE);
		}

	std::sort(m_list.begin(), m_list.end(), compareEngines );
	return true;
}

bool CUploadEngineList::compareEngines(const CUploadEngineData& elem1, const CUploadEngineData& elem2)
{
	return IuStringUtils::stricmp(elem1.Name.c_str(), elem2.Name.c_str()) < 0;
}

void CUploadEngineList::setNumOfRetries(int Engine, int Action)
{
	m_EngineNumOfRetries = Engine;
	m_ActionNumOfRetries = Action;
}

bool CUploadEngineList::addServer(const CUploadEngineData& data)
{
	m_list.push_back(data);
	std::sort(m_list.begin(), m_list.end(), compareEngines );
	return true;
}
