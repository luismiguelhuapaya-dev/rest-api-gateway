#pragma once

#include "gateway/Common.h"
#include "gateway/transport/HttpParser.h"
#include "gateway/routing/EndpointDefinition.h"
#include "gateway/routing/ParameterSchema.h"
#include "gateway/validation/JsonParser.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Gateway
{

    struct ValidationError
    {
        std::string     m_szParameterName;
        std::string     m_szErrorMessage;
        std::string     m_szProvidedValue;
    };

    struct ValidationResult
    {
        bool                            m_bIsValid;
        std::vector<ValidationError>    m_stdsErrors;
        std::string                     m_szFormattedErrorMessage;
    };

    class ValidationEngine
    {
        public:
            ValidationEngine();
            ~ValidationEngine() = default;

            ValidationResult __thiscall ValidateRequest(
                _in const HttpRequest& sRequest,
                _in const EndpointDefinition& sEndpointDefinition
            ) const;

        private:
            bool __thiscall ValidatePathParameters(
                _in const HttpRequest& sRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out std::vector<ValidationError>& stdsErrors
            ) const;

            bool __thiscall ValidateQueryParameters(
                _in const HttpRequest& sRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out std::vector<ValidationError>& stdsErrors
            ) const;

            bool __thiscall ValidateHeaderParameters(
                _in const HttpRequest& sRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out std::vector<ValidationError>& stdsErrors
            ) const;

            bool __thiscall ValidateBodyParameters(
                _in const HttpRequest& sRequest,
                _in const EndpointDefinition& sEndpointDefinition,
                _out std::vector<ValidationError>& stdsErrors
            ) const;

            bool __thiscall ValidateJsonBodyField(
                _in const JsonValue& sJsonBody,
                _in const ParameterSchema& sSchema,
                _out std::vector<ValidationError>& stdsErrors
            ) const;

            bool __thiscall ValidateParameterValue(
                _in const std::string& szValue,
                _in const ParameterSchema& sSchema,
                _out std::string& szErrorMessage
            ) const;

            std::string __thiscall FormatValidationErrors(
                _in const std::vector<ValidationError>& stdsErrors
            ) const;
    };

} // namespace Gateway
