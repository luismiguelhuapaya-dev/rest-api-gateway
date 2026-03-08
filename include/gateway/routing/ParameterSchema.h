#pragma once

#include "gateway/Common.h"
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <variant>

namespace Gateway
{

    struct ParameterConstraints
    {
        std::optional<int64_t>          m_stdn64MinValue;
        std::optional<int64_t>          m_stdn64MaxValue;
        std::optional<double>           m_stdfl64MinValue;
        std::optional<double>           m_stdfl64MaxValue;
        std::optional<uint32_t>         m_stdun32MinLength;
        std::optional<uint32_t>         m_stdun32MaxLength;
        std::optional<std::string>      m_stdszPattern;
        std::vector<std::string>        m_stdszAllowedValues;
        std::optional<uint32_t>         m_stdun32MinArrayItems;
        std::optional<uint32_t>         m_stdun32MaxArrayItems;
    };

    struct ParameterSchema
    {
        std::string             m_szName;
        ParameterType           m_eParameterType;
        ParameterLocation       m_eLocation;
        bool                    m_bIsRequired;
        std::string             m_szDescription;
        std::string             m_szDefaultValue;
        ParameterConstraints    m_sConstraints;

        bool __thiscall ValidateValue(
            _in const std::string& szValue,
            _out std::string& szErrorMessage
        ) const;

        bool __thiscall IsValid() const;
    };

    bool __stdcall ParseParameterSchemaFromJson(
        _in const std::string& szJson,
        _out ParameterSchema& sSchema
    );

    bool __stdcall ValidateStringConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    );

    bool __stdcall ValidateIntegerConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    );

    bool __stdcall ValidateFloatConstraints(
        _in const std::string& szValue,
        _in const ParameterConstraints& sConstraints,
        _out std::string& szErrorMessage
    );

    bool __stdcall ValidateBooleanValue(
        _in const std::string& szValue,
        _out std::string& szErrorMessage
    );

} // namespace Gateway
