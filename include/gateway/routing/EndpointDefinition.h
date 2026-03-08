#pragma once

#include "gateway/Common.h"
#include "gateway/routing/ParameterSchema.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Gateway
{

    struct EndpointDefinition
    {
        std::string                             m_szPath;
        HttpMethod                              m_eMethod;
        std::string                             m_szBackendIdentifier;
        std::string                             m_szDescription;
        bool                                    m_bRequiresAuthentication;
        std::vector<ParameterSchema>            m_stdsParameterSchemas;
        std::vector<std::string>                m_stdszPathSegments;
        std::vector<bool>                       m_stdbIsParameterSegment;
        std::vector<std::string>                m_stdszParameterNames;

        bool __thiscall MatchesPath(
            _in const std::string& szRequestPath,
            _out std::unordered_map<std::string, std::string>& stdszszPathParameters
        ) const;

        bool __thiscall IsValid() const;

        std::string __thiscall GetRouteKey() const;
    };

    class EndpointDefinitionBuilder
    {
        public:
            EndpointDefinitionBuilder();
            ~EndpointDefinitionBuilder() = default;

            EndpointDefinitionBuilder& __thiscall SetPath(
                _in const std::string& szPath
            );

            EndpointDefinitionBuilder& __thiscall SetMethod(
                _in HttpMethod eMethod
            );

            EndpointDefinitionBuilder& __thiscall SetBackendIdentifier(
                _in const std::string& szBackendIdentifier
            );

            EndpointDefinitionBuilder& __thiscall SetDescription(
                _in const std::string& szDescription
            );

            EndpointDefinitionBuilder& __thiscall SetRequiresAuthentication(
                _in bool bRequiresAuthentication
            );

            EndpointDefinitionBuilder& __thiscall AddParameterSchema(
                _in const ParameterSchema& sParameterSchema
            );

            EndpointDefinition __thiscall Build() const;

        private:
            void __thiscall ParsePathSegments(
                _inout EndpointDefinition& sDefinition
            ) const;

            EndpointDefinition      m_sDefinition;
    };

    bool __stdcall ParseEndpointDefinitionFromJson(
        _in const std::string& szJson,
        _out EndpointDefinition& sDefinition
    );

} // namespace Gateway
