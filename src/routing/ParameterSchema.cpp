#include "gateway/routing/ParameterSchema.h"
#include "gateway/validation/JsonParser.h"
#include <regex>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cmath>

namespace Gateway
{

    bool __thiscall ParameterSchema::ValidateValue(
        _in const std::string& szValue,
        _out std::string& szErrorMessage
    ) const
    {
        bool bResult = false;

        if (m_eParameterType == ParameterType::String)
        {
            bResult = ValidateStringConstraints(szValue, m_sConstraints, szErrorMessage);
        }
        else if (m_eParameterType == ParameterType::Integer)
        {
            bResult = ValidateIntegerConstraints(szValue, m_sConstraints, szErrorMessage);
        }
        else if (m_eParameterType == ParameterType::Float)
        {
            bResult = ValidateFloatConstraints(szValue, m_sConstraints, szErrorMessage);
        }
        else if (m_eParameterType == ParameterType::Boolean)
        {
            bResult = ValidateBooleanValue(szValue, szErrorMessage);
        }
        else
        {
            // Object and Array types are validated at a higher level (JSON body validation)
            bResult = true;
        }

        return bResult;
    }

    bool __thiscall ParameterSchema::IsValid() const
    {
        bool bResult = false;

        if (!m_szName.empty())
        {
            bResult = true;
        }

        return bResult;
    }

    bool __stdcall ParseParameterSchemaFromJson(
        _in const std::string& szJson,
        _out ParameterSchema& sSchema
    )
    {
        bool bResult = false;

        JsonParser sParser;
        JsonValue sRoot;

        if (sParser.Parse(szJson, sRoot))
        {
            if (sRoot.IsObject())
            {
                if (sRoot.HasMember("name"))
                {
                    sSchema.m_szName = sRoot.GetMember("name").GetString();
                }

                if (sRoot.HasMember("type"))
                {
                    std::string szType = sRoot.GetMember("type").GetString();
                    if (szType == "string")
                    {
                        sSchema.m_eParameterType = ParameterType::String;
                    }
                    else if (szType == "integer")
                    {
                        sSchema.m_eParameterType = ParameterType::Integer;
                    }
                    else if (szType == "float")
                    {
                        sSchema.m_eParameterType = ParameterType::Float;
                    }
                    else if (szType == "boolean")
                    {
                        sSchema.m_eParameterType = ParameterType::Boolean;
                    }
                    else if (szType == "object")
                    {
                        sSchema.m_eParameterType = ParameterType::Object;
                    }
                    else if (szType == "array")
                    {
                        sSchema.m_eParameterType = ParameterType::Array;
                    }
                }

                if (sRoot.HasMember("location"))
                {
                    std::string szLocation = sRoot.GetMember("location").GetString();
                    if (szLocation == "path")
                    {
                        sSchema.m_eLocation = ParameterLocation::Path;
                    }
                    else if (szLocation == "query")
                    {
                        sSchema.m_eLocation = ParameterLocation::Query;
                    }
                    else if (szLocation == "header")
                    {
                        sSchema.m_eLocation = ParameterLocation::Header;
                    }
                    else if (szLocation == "body")
                    {
                        sSchema.m_eLocation = ParameterLocation::Body;
                    }
                }

                if (sRoot.HasMember("required"))
                {
                    sSchema.m_bIsRequired = sRoot.GetMember("required").GetBoolean();
                }
                else
                {
                    sSchema.m_bIsRequired = false;
                }

                if (sRoot.HasMember("description"))
                {
                    sSchema.m_szDescription = sRoot.GetMember("description").GetString();
                }

                if (sRoot.HasMember("default"))
                {
                    sSchema.m_szDefaultValue = sRoot.GetMember("default").GetString();
                }

                // Parse constraints
                if (sRoot.HasMember("constraints"))
                {
                    JsonValue sConstraints = sRoot.GetMember("constraints");
                    if (sConstraints.IsObject())
                    {
                        if (sConstraints.HasMember("min_value"))
                        {
                            sSchema.m_sConstraints.m_stdn64MinValue = sConstraints.GetMember("min_value").GetInteger();
                        }
                        if (sConstraints.HasMember("max_value"))
                        {
                            sSchema.m_sConstraints.m_stdn64MaxValue = sConstraints.GetMember("max_value").GetInteger();
                        }
                        if (sConstraints.HasMember("min_length"))
                        {
                            sSchema.m_sConstraints.m_stdun32MinLength = static_cast<uint32_t>(sConstraints.GetMember("min_length").GetInteger());
                        }
                        if (sConstraints.HasMember("max_length"))
                        {
                            sSchema.m_sConstraints.m_stdun32MaxLength = static_cast<uint32_t>(sConstraints.GetMember("max_length").GetInteger());
                        }
                        if (sConstraints.HasMember("pattern"))
                        {
                            sSchema.m_sConstraints.m_stdszPattern = sConstraints.GetMember("pattern").GetString();
                        }
                        if (sConstraints.HasMember("allowed_values"))
                        {
                            JsonValue sAllowedValues = sConstraints.GetMember("allowed_values");
                            if (sAllowedValues.IsArray())
                            {
                                for (uint32_t un32Index = 0; (un32Index < sAllowedValues.GetArraySize()); ++un32Index)
                                {
                                    sSchema.m_sConstraints.m_stdszAllowedValues.push_back(
                                        sAllowedValues.GetArrayElement(un32Index).GetString()
                                    );
                                }
                            }
                        }
                    }
                }

                bResult = sSchema.IsValid();
            }
        }

        return bResult;
    }

    bool __stdcall ValidateStringConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    )
    {
        bool bResult = true;

        if ((bResult) && (sConstraints.m_stdun32MinLength.has_value()))
        {
            if (szValue.size() < sConstraints.m_stdun32MinLength.value())
            {
                szErrorMessage = "String length " + std::to_string(szValue.size()) +
                    " is below minimum " + std::to_string(sConstraints.m_stdun32MinLength.value());
                bResult = false;
            }
        }

        if ((bResult) && (sConstraints.m_stdun32MaxLength.has_value()))
        {
            if (szValue.size() > sConstraints.m_stdun32MaxLength.value())
            {
                szErrorMessage = "String length " + std::to_string(szValue.size()) +
                    " exceeds maximum " + std::to_string(sConstraints.m_stdun32MaxLength.value());
                bResult = false;
            }
        }

        if ((bResult) && (sConstraints.m_stdszPattern.has_value()))
        {
            try
            {
                std::regex stdPattern(sConstraints.m_stdszPattern.value());
                if (!std::regex_match(szValue, stdPattern))
                {
                    szErrorMessage = "String does not match required pattern";
                    bResult = false;
                }
            }
            catch (const std::regex_error&)
            {
                szErrorMessage = "Invalid regex pattern in schema";
                bResult = false;
            }
        }

        if ((bResult) && (!sConstraints.m_stdszAllowedValues.empty()))
        {
            bool bFound = false;
            for (const auto& szAllowed : sConstraints.m_stdszAllowedValues)
            {
                if (szValue == szAllowed)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                szErrorMessage = "Value is not in the list of allowed values";
                bResult = false;
            }
        }

        return bResult;
    }

    bool __stdcall ValidateIntegerConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    )
    {
        bool bResult = false;

        char* pszEnd = nullptr;
        errno = 0;
        int64_t n64Value = std::strtoll(szValue.c_str(), &pszEnd, 10);

        if ((pszEnd != szValue.c_str()) && (*pszEnd == '\0') && (errno == 0))
        {
            bResult = true;

            if ((bResult) && (sConstraints.m_stdn64MinValue.has_value()))
            {
                if (n64Value < sConstraints.m_stdn64MinValue.value())
                {
                    szErrorMessage = "Integer value " + std::to_string(n64Value) +
                        " is below minimum " + std::to_string(sConstraints.m_stdn64MinValue.value());
                    bResult = false;
                }
            }

            if ((bResult) && (sConstraints.m_stdn64MaxValue.has_value()))
            {
                if (n64Value > sConstraints.m_stdn64MaxValue.value())
                {
                    szErrorMessage = "Integer value " + std::to_string(n64Value) +
                        " exceeds maximum " + std::to_string(sConstraints.m_stdn64MaxValue.value());
                    bResult = false;
                }
            }
        }
        else
        {
            szErrorMessage = "Value is not a valid integer";
        }

        return bResult;
    }

    bool __stdcall ValidateFloatConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    )
    {
        bool bResult = false;

        char* pszEnd = nullptr;
        errno = 0;
        double fl64Value = std::strtod(szValue.c_str(), &pszEnd);

        if ((pszEnd != szValue.c_str()) && (*pszEnd == '\0') && (errno == 0) && (!std::isinf(fl64Value)) && (!std::isnan(fl64Value)))
        {
            bResult = true;

            if ((bResult) && (sConstraints.m_stdfl64MinValue.has_value()))
            {
                if (fl64Value < sConstraints.m_stdfl64MinValue.value())
                {
                    szErrorMessage = "Float value is below minimum";
                    bResult = false;
                }
            }

            if ((bResult) && (sConstraints.m_stdfl64MaxValue.has_value()))
            {
                if (fl64Value > sConstraints.m_stdfl64MaxValue.value())
                {
                    szErrorMessage = "Float value exceeds maximum";
                    bResult = false;
                }
            }
        }
        else
        {
            szErrorMessage = "Value is not a valid floating-point number";
        }

        return bResult;
    }

    bool __stdcall ValidateBooleanValue(
        _in const std::string& szValue,
        _out std::string& szErrorMessage
    )
    {
        bool bResult = false;

        if ((szValue == "true") || (szValue == "false") ||
            (szValue == "1") || (szValue == "0") ||
            (szValue == "yes") || (szValue == "no"))
        {
            bResult = true;
        }
        else
        {
            szErrorMessage = "Value is not a valid boolean (expected true/false, 1/0, or yes/no)";
        }

        return bResult;
    }

} // namespace Gateway
