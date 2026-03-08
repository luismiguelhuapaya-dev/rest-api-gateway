#pragma once

#include "gateway/Common.h"
#include "gateway/routing/EndpointDefinition.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <optional>

namespace Gateway
{

    class RoutingTable
    {
        public:
            RoutingTable();
            ~RoutingTable() = default;

            bool __thiscall RegisterEndpoint(
                _in const EndpointDefinition& sDefinition
            );

            bool __thiscall UnregisterEndpoint(
                _in const std::string& szPath,
                _in HttpMethod eMethod
            );

            bool __thiscall FindEndpoint(
                _in const std::string& szPath,
                _in HttpMethod eMethod,
                _out EndpointDefinition& sDefinition,
                _out std::unordered_map<std::string, std::string>& stdszszPathParameters
            ) const;

            bool __thiscall HasEndpoint(
                _in const std::string& szPath,
                _in HttpMethod eMethod
            ) const;

            uint32_t __thiscall GetEndpointCount() const;

            std::vector<EndpointDefinition> __thiscall GetAllEndpoints() const;

            std::vector<EndpointDefinition> __thiscall GetEndpointsByBackend(
                _in const std::string& szBackendIdentifier
            ) const;

            uint32_t __thiscall RemoveEndpointsByBackend(
                _in const std::string& szBackendIdentifier
            );

            void __thiscall Clear();

        private:
            std::vector<EndpointDefinition>                     m_stdsEndpoints;
            std::unordered_map<std::string, uint32_t>           m_stdszun32RouteIndex;
            mutable std::mutex                                  m_stdMutexAccess;
            static const uint32_t                               msc_un32MaxEndpoints = 4096;
    };

} // namespace Gateway
