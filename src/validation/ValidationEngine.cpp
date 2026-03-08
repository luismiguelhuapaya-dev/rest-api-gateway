#include "gateway/validation/ValidationEngine.h"
#include "gateway/logging/Logger.h"
#include <sstream>
#include <algorithm>

namespace Gateway
{

    ValidationEngine::ValidationEngine()
    {
    }

    ValidationResult __thiscall ValidationEngine::ValidateRequest(
        _in const HttpRequest& sRequest,
        _in const EndpointDefinition& sEndpointDefinition
    ) const
    {
        ValidationResult sResult;
        sResult.m_bIsValid = true;

        std::vector<ValidationError> stdsErrors;

        if (!ValidatePathParameters(sRequest, sEndpointDefinition, stdsErrors))
        {
            sResult.m_bIsValid = false;
        }

        if (!ValidateQueryParameters(sRequest, sEndpointDefinition, stdsErrors))
        {
            sResult.m_bIsValid = false;
        }

        if (!ValidateHeaderParameters(sRequest, sEndpointDefinition, stdsErrors))
        {
            sResult.m_bIsValid = false;
        }

        if (!ValidateBodyParameters(sRequest, sEndpointDefinition, stdsErrors))
        {
            sResult.m_bIsValid = false;
        }

        sResult.m_stdsErrors = stdsErrors;
        if (!sResult.m_bIsValid)
        {
            sResult.m_szFormattedErrorMessage = FormatValidationErrors(stdsErrors);
        }

        return sResult;
    }

    bool __thiscall ValidationEngine::ValidatePathParameters(
        _in const HttpRequest& sRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out std::vector<ValidationError>& stdsErrors
    ) const
    {
        bool bResult = true;

        for (const auto& sSchema : sEndpointDefinition.m_stdsParameterSchemas)
        {
            if (sSchema.m_eLocation == ParameterLocation::Path)
            {
                auto stdIterator = sRequest.m_stdszszPathParameters.find(sSchema.m_szName);
                if (stdIterator != sRequest.m_stdszszPathParameters.end())
                {
                    std::string szErrorMessage;
                    if (!ValidateParameterValue(stdIterator->second, sSchema, szErrorMessage))
                    {
                        ValidationError sError;
                        sError.m_szParameterName = sSchema.m_szName;
                        sError.m_szErrorMessage = szErrorMessage;
                        sError.m_szProvidedValue = stdIterator->second;
                        stdsErrors.push_back(sError);
                        bResult = false;
                    }
                }
                else if (sSchema.m_bIsRequired)
                {
                    ValidationError sError;
                    sError.m_szParameterName = sSchema.m_szName;
                    sError.m_szErrorMessage = "Required path parameter is missing";
                    stdsErrors.push_back(sError);
                    bResult = false;
                }
            }
        }

        return bResult;
    }

    bool __thiscall ValidationEngine::ValidateQueryParameters(
        _in const HttpRequest& sRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out std::vector<ValidationError>& stdsErrors
    ) const
    {
        bool bResult = true;

        for (const auto& sSchema : sEndpointDefinition.m_stdsParameterSchemas)
        {
            if (sSchema.m_eLocation == ParameterLocation::Query)
            {
                auto stdIterator = sRequest.m_stdszszQueryParameters.find(sSchema.m_szName);
                if (stdIterator != sRequest.m_stdszszQueryParameters.end())
                {
                    std::string szErrorMessage;
                    if (!ValidateParameterValue(stdIterator->second, sSchema, szErrorMessage))
                    {
                        ValidationError sError;
                        sError.m_szParameterName = sSchema.m_szName;
                        sError.m_szErrorMessage = szErrorMessage;
                        sError.m_szProvidedValue = stdIterator->second;
                        stdsErrors.push_back(sError);
                        bResult = false;
                    }
                }
                else if (sSchema.m_bIsRequired)
                {
                    ValidationError sError;
                    sError.m_szParameterName = sSchema.m_szName;
                    sError.m_szErrorMessage = "Required query parameter is missing";
                    stdsErrors.push_back(sError);
                    bResult = false;
                }
            }
        }

        return bResult;
    }

    bool __thiscall ValidationEngine::ValidateHeaderParameters(
        _in const HttpRequest& sRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out std::vector<ValidationError>& stdsErrors
    ) const
    {
        bool bResult = true;

        for (const auto& sSchema : sEndpointDefinition.m_stdsParameterSchemas)
        {
            if (sSchema.m_eLocation == ParameterLocation::Header)
            {
                // Headers are stored in lowercase
                std::string szLowerName = sSchema.m_szName;
                std::transform(szLowerName.begin(), szLowerName.end(), szLowerName.begin(),
                    [](unsigned char chCharacter)
                    {
                        return std::tolower(chCharacter);
                    }
                );

                auto stdIterator = sRequest.m_stdszszHeaders.find(szLowerName);
                if (stdIterator != sRequest.m_stdszszHeaders.end())
                {
                    std::string szErrorMessage;
                    if (!ValidateParameterValue(stdIterator->second, sSchema, szErrorMessage))
                    {
                        ValidationError sError;
                        sError.m_szParameterName = sSchema.m_szName;
                        sError.m_szErrorMessage = szErrorMessage;
                        sError.m_szProvidedValue = stdIterator->second;
                        stdsErrors.push_back(sError);
                        bResult = false;
                    }
                }
                else if (sSchema.m_bIsRequired)
                {
                    ValidationError sError;
                    sError.m_szParameterName = sSchema.m_szName;
                    sError.m_szErrorMessage = "Required header parameter is missing";
                    stdsErrors.push_back(sError);
                    bResult = false;
                }
            }
        }

        return bResult;
    }

    bool __thiscall ValidationEngine::ValidateBodyParameters(
        _in const HttpRequest& sRequest,
        _in const EndpointDefinition& sEndpointDefinition,
        _out std::vector<ValidationError>& stdsErrors
    ) const
    {
        bool bResult = true;

        // Collect body schemas
        std::vector<const ParameterSchema*> stdpsBodySchemas;
        for (const auto& sSchema : sEndpointDefinition.m_stdsParameterSchemas)
        {
            if (sSchema.m_eLocation == ParameterLocation::Body)
            {
                stdpsBodySchemas.push_back(&sSchema);
            }
        }

        if (!stdpsBodySchemas.empty())
        {
            if (sRequest.m_szBody.empty())
            {
                // Check if any body parameters are required
                for (const auto* psSchema : stdpsBodySchemas)
                {
                    if (psSchema->m_bIsRequired)
                    {
                        ValidationError sError;
                        sError.m_szParameterName = psSchema->m_szName;
                        sError.m_szErrorMessage = "Required body parameter is missing (no body provided)";
                        stdsErrors.push_back(sError);
                        bResult = false;
                    }
                }
            }
            else
            {
                // Parse the body as JSON
                JsonParser sParser;
                JsonValue sJsonBody;

                if (sParser.Parse(sRequest.m_szBody, sJsonBody))
                {
                    for (const auto* psSchema : stdpsBodySchemas)
                    {
                        if (!ValidateJsonBodyField(sJsonBody, *psSchema, stdsErrors))
                        {
                            bResult = false;
                        }
                    }
                }
                else
                {
                    ValidationError sError;
                    sError.m_szParameterName = "body";
                    sError.m_szErrorMessage = "Request body is not valid JSON: " + sParser.GetErrorMessage();
                    stdsErrors.push_back(sError);
                    bResult = false;
                }
            }
        }

        return bResult;
    }

    bool __thiscall ValidationEngine::ValidateJsonBodyField(
        _in const JsonValue& sJsonBody,
        _in const ParameterSchema& sSchema,
        _out std::vector<ValidationError>& stdsErrors
    ) const
    {
        bool bResult = true;

        if (sJsonBody.IsObject())
        {
            if (sJsonBody.HasMember(sSchema.m_szName))
            {
                JsonValue sFieldValue = sJsonBody.GetMember(sSchema.m_szName);

                // Type check
                bool bTypeValid = true;
                std::string szFieldValueString;

                if (sSchema.m_eParameterType == ParameterType::String)
                {
                    if (sFieldValue.IsString())
                    {
                        szFieldValueString = sFieldValue.GetString();
                    }
                    else
                    {
                        bTypeValid = false;
                    }
                }
                else if (sSchema.m_eParameterType == ParameterType::Integer)
                {
                    if (sFieldValue.IsInteger())
                    {
                        szFieldValueString = std::to_string(sFieldValue.GetInteger());
                    }
                    else
                    {
                        bTypeValid = false;
                    }
                }
                else if (sSchema.m_eParameterType == ParameterType::Float)
                {
                    if ((sFieldValue.IsFloat()) || (sFieldValue.IsInteger()))
                    {
                        if (sFieldValue.IsFloat())
                        {
                            szFieldValueString = std::to_string(sFieldValue.GetFloat());
                        }
                        else
                        {
                            szFieldValueString = std::to_string(sFieldValue.GetInteger());
                        }
                    }
                    else
                    {
                        bTypeValid = false;
                    }
                }
                else if (sSchema.m_eParameterType == ParameterType::Boolean)
                {
                    if (sFieldValue.IsBoolean())
                    {
                        szFieldValueString = sFieldValue.GetBoolean() ? "true" : "false";
                    }
                    else
                    {
                        bTypeValid = false;
                    }
                }
                else if (sSchema.m_eParameterType == ParameterType::Array)
                {
                    if (!sFieldValue.IsArray())
                    {
                        bTypeValid = false;
                    }
                }
                else if (sSchema.m_eParameterType == ParameterType::Object)
                {
                    if (!sFieldValue.IsObject())
                    {
                        bTypeValid = false;
                    }
                }

                if (!bTypeValid)
                {
                    ValidationError sError;
                    sError.m_szParameterName = sSchema.m_szName;
                    sError.m_szErrorMessage = "Field type mismatch";
                    stdsErrors.push_back(sError);
                    bResult = false;
                }
                else if ((!szFieldValueString.empty()) &&
                         ((sSchema.m_eParameterType == ParameterType::String) ||
                          (sSchema.m_eParameterType == ParameterType::Integer) ||
                          (sSchema.m_eParameterType == ParameterType::Float)))
                {
                    std::string szErrorMessage;
                    if (!ValidateParameterValue(szFieldValueString, sSchema, szErrorMessage))
                    {
                        ValidationError sError;
                        sError.m_szParameterName = sSchema.m_szName;
                        sError.m_szErrorMessage = szErrorMessage;
                        sError.m_szProvidedValue = szFieldValueString;
                        stdsErrors.push_back(sError);
                        bResult = false;
                    }
                }
            }
            else if (sSchema.m_bIsRequired)
            {
                ValidationError sError;
                sError.m_szParameterName = sSchema.m_szName;
                sError.m_szErrorMessage = "Required body field is missing";
                stdsErrors.push_back(sError);
                bResult = false;
            }
        }
        else if (sSchema.m_bIsRequired)
        {
            ValidationError sError;
            sError.m_szParameterName = sSchema.m_szName;
            sError.m_szErrorMessage = "Expected JSON object body";
            stdsErrors.push_back(sError);
            bResult = false;
        }

        return bResult;
    }

    bool __thiscall ValidationEngine::ValidateParameterValue(
        _in const std::string& szValue,
        _in const ParameterSchema& sSchema,
        _out std::string& szErrorMessage
    ) const
    {
        bool bResult = sSchema.ValidateValue(szValue, szErrorMessage);
        return bResult;
    }

    std::string __thiscall ValidationEngine::FormatValidationErrors(
        _in const std::vector<ValidationError>& stdsErrors
    ) const
    {
        std::string szResult = "{\"errors\":[";

        for (size_t un64Index = 0; (un64Index < stdsErrors.size()); ++un64Index)
        {
            if (un64Index > 0)
            {
                szResult += ",";
            }
            szResult += "{\"parameter\":\"" + stdsErrors[un64Index].m_szParameterName +
                        "\",\"message\":\"" + stdsErrors[un64Index].m_szErrorMessage + "\"";
            if (!stdsErrors[un64Index].m_szProvidedValue.empty())
            {
                szResult += ",\"provided_value\":\"" + stdsErrors[un64Index].m_szProvidedValue + "\"";
            }
            szResult += "}";
        }

        szResult += "]}";
        return szResult;
    }

} // namespace Gateway
