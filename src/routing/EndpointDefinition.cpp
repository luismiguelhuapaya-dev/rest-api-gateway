#include "gateway/routing/EndpointDefinition.h"
#include "gateway/validation/JsonParser.h"
#include <sstream>

namespace Gateway
{

    bool __thiscall EndpointDefinition::MatchesPath(
        _in const std::string& szRequestPath,
        _out std::unordered_map<std::string, std::string>& stdszszPathParameters
    ) const
    {
        bool bResult = false;

        // Split the request path into segments
        std::vector<std::string> stdszRequestSegments;
        std::istringstream stdStream(szRequestPath);
        std::string szSegment;

        while (std::getline(stdStream, szSegment, '/'))
        {
            if (!szSegment.empty())
            {
                stdszRequestSegments.push_back(szSegment);
            }
        }

        // Compare segment counts
        if (stdszRequestSegments.size() == m_stdszPathSegments.size())
        {
            bResult = true;
            stdszszPathParameters.clear();

            for (size_t un64Index = 0; ((un64Index < m_stdszPathSegments.size()) && (bResult)); ++un64Index)
            {
                if (m_stdbIsParameterSegment[un64Index])
                {
                    // This is a parameter placeholder - extract the value
                    stdszszPathParameters[m_stdszParameterNames[un64Index]] = stdszRequestSegments[un64Index];
                }
                else
                {
                    // This is a literal segment - must match exactly
                    if (m_stdszPathSegments[un64Index] != stdszRequestSegments[un64Index])
                    {
                        bResult = false;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall EndpointDefinition::IsValid() const
    {
        bool bResult = false;

        if ((!m_szPath.empty()) &&
            (m_eMethod != HttpMethod::Unknown))
        {
            bResult = true;
        }

        return bResult;
    }

    std::string __thiscall EndpointDefinition::GetRouteKey() const
    {
        std::string szResult = HttpMethodToString(m_eMethod) + ":" + m_szPath;
        return szResult;
    }

    // EndpointDefinitionBuilder implementation
    EndpointDefinitionBuilder::EndpointDefinitionBuilder()
    {
        m_sDefinition.m_eMethod = HttpMethod::Unknown;
        m_sDefinition.m_bRequiresAuthentication = false;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::SetPath(
        _in const std::string& szPath
    )
    {
        m_sDefinition.m_szPath = szPath;
        return *this;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::SetMethod(
        _in HttpMethod eMethod
    )
    {
        m_sDefinition.m_eMethod = eMethod;
        return *this;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::SetBackendIdentifier(
        _in const std::string& szBackendIdentifier
    )
    {
        m_sDefinition.m_szBackendIdentifier = szBackendIdentifier;
        return *this;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::SetDescription(
        _in const std::string& szDescription
    )
    {
        m_sDefinition.m_szDescription = szDescription;
        return *this;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::SetRequiresAuthentication(
        _in bool bRequiresAuthentication
    )
    {
        m_sDefinition.m_bRequiresAuthentication = bRequiresAuthentication;
        return *this;
    }

    EndpointDefinitionBuilder& __thiscall EndpointDefinitionBuilder::AddParameterSchema(
        _in const ParameterSchema& sParameterSchema
    )
    {
        m_sDefinition.m_stdsParameterSchemas.push_back(sParameterSchema);
        return *this;
    }

    EndpointDefinition __thiscall EndpointDefinitionBuilder::Build() const
    {
        EndpointDefinition sResult = m_sDefinition;
        ParsePathSegments(sResult);
        return sResult;
    }

    void __thiscall EndpointDefinitionBuilder::ParsePathSegments(
        _inout EndpointDefinition& sDefinition
    ) const
    {
        sDefinition.m_stdszPathSegments.clear();
        sDefinition.m_stdbIsParameterSegment.clear();
        sDefinition.m_stdszParameterNames.clear();

        std::istringstream stdStream(sDefinition.m_szPath);
        std::string szSegment;

        while (std::getline(stdStream, szSegment, '/'))
        {
            if (!szSegment.empty())
            {
                sDefinition.m_stdszPathSegments.push_back(szSegment);

                if ((szSegment.front() == '{') && (szSegment.back() == '}'))
                {
                    sDefinition.m_stdbIsParameterSegment.push_back(true);
                    sDefinition.m_stdszParameterNames.push_back(
                        szSegment.substr(1, szSegment.size() - 2)
                    );
                }
                else
                {
                    sDefinition.m_stdbIsParameterSegment.push_back(false);
                    sDefinition.m_stdszParameterNames.push_back("");
                }
            }
        }
    }

    bool __stdcall ParseEndpointDefinitionFromJson(
        _in const std::string& szJson,
        _out EndpointDefinition& sDefinition
    )
    {
        bool bResult = false;

        JsonParser sParser;
        JsonValue sRoot;

        if (sParser.Parse(szJson, sRoot))
        {
            if (sRoot.IsObject())
            {
                EndpointDefinitionBuilder sBuilder;

                if (sRoot.HasMember("path"))
                {
                    sBuilder.SetPath(sRoot.GetMember("path").GetString());
                }

                if (sRoot.HasMember("method"))
                {
                    std::string szMethod = sRoot.GetMember("method").GetString();
                    sBuilder.SetMethod(StringToHttpMethod(szMethod));
                }

                if (sRoot.HasMember("description"))
                {
                    sBuilder.SetDescription(sRoot.GetMember("description").GetString());
                }

                if (sRoot.HasMember("requires_auth"))
                {
                    sBuilder.SetRequiresAuthentication(sRoot.GetMember("requires_auth").GetBoolean());
                }

                if (sRoot.HasMember("parameters"))
                {
                    JsonValue sParameters = sRoot.GetMember("parameters");
                    if (sParameters.IsArray())
                    {
                        for (uint32_t un32Index = 0; (un32Index < sParameters.GetArraySize()); ++un32Index)
                        {
                            JsonValue sParamJson = sParameters.GetArrayElement(un32Index);
                            std::string szParamJsonStr = sParamJson.Serialize();

                            ParameterSchema sSchema;
                            if (ParseParameterSchemaFromJson(szParamJsonStr, sSchema))
                            {
                                sBuilder.AddParameterSchema(sSchema);
                            }
                        }
                    }
                }

                sDefinition = sBuilder.Build();
                bResult = sDefinition.IsValid();
            }
        }

        return bResult;
    }

} // namespace Gateway
