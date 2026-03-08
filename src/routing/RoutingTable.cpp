#include "gateway/routing/RoutingTable.h"
#include "gateway/logging/Logger.h"

namespace Gateway
{

    RoutingTable::RoutingTable()
    {
    }

    bool __thiscall RoutingTable::RegisterEndpoint(
        _in const EndpointDefinition& sDefinition
    )
    {
        bool bResult = false;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        if (sDefinition.IsValid())
        {
            std::string szRouteKey = sDefinition.GetRouteKey();

            auto stdIterator = m_stdszun32RouteIndex.find(szRouteKey);
            if (stdIterator == m_stdszun32RouteIndex.end())
            {
                if (m_stdsEndpoints.size() < msc_un32MaxEndpoints)
                {
                    uint32_t un32Index = static_cast<uint32_t>(m_stdsEndpoints.size());
                    m_stdsEndpoints.push_back(sDefinition);
                    m_stdszun32RouteIndex[szRouteKey] = un32Index;
                    bResult = true;

                    GATEWAY_LOG_INFO("RoutingTable", "Registered endpoint: " + szRouteKey);
                }
                else
                {
                    GATEWAY_LOG_ERROR("RoutingTable", "Maximum endpoint count reached");
                }
            }
            else
            {
                // Update existing endpoint
                m_stdsEndpoints[stdIterator->second] = sDefinition;
                bResult = true;
                GATEWAY_LOG_INFO("RoutingTable", "Updated endpoint: " + szRouteKey);
            }
        }

        return bResult;
    }

    bool __thiscall RoutingTable::UnregisterEndpoint(
        _in const std::string& szPath,
        _in HttpMethod eMethod
    )
    {
        bool bResult = false;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        std::string szRouteKey = HttpMethodToString(eMethod) + ":" + szPath;
        auto stdIterator = m_stdszun32RouteIndex.find(szRouteKey);

        if (stdIterator != m_stdszun32RouteIndex.end())
        {
            uint32_t un32IndexToRemove = stdIterator->second;

            // Swap with last element and pop
            if (un32IndexToRemove < (m_stdsEndpoints.size() - 1))
            {
                std::string szLastRouteKey = m_stdsEndpoints.back().GetRouteKey();
                m_stdsEndpoints[un32IndexToRemove] = std::move(m_stdsEndpoints.back());
                m_stdszun32RouteIndex[szLastRouteKey] = un32IndexToRemove;
            }

            m_stdsEndpoints.pop_back();
            m_stdszun32RouteIndex.erase(stdIterator);
            bResult = true;

            GATEWAY_LOG_INFO("RoutingTable", "Unregistered endpoint: " + szRouteKey);
        }

        return bResult;
    }

    bool __thiscall RoutingTable::FindEndpoint(
        _in const std::string& szPath,
        _in HttpMethod eMethod,
        _out EndpointDefinition& sDefinition,
        _out std::unordered_map<std::string, std::string>& stdszszPathParameters
    ) const
    {
        bool bResult = false;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        // First try exact match
        std::string szRouteKey = HttpMethodToString(eMethod) + ":" + szPath;
        auto stdIterator = m_stdszun32RouteIndex.find(szRouteKey);

        if (stdIterator != m_stdszun32RouteIndex.end())
        {
            sDefinition = m_stdsEndpoints[stdIterator->second];
            bResult = true;
        }
        else
        {
            // Try pattern matching (for endpoints with path parameters)
            for (const auto& sEndpoint : m_stdsEndpoints)
            {
                if (sEndpoint.m_eMethod == eMethod)
                {
                    if (sEndpoint.MatchesPath(szPath, stdszszPathParameters))
                    {
                        sDefinition = sEndpoint;
                        bResult = true;
                        break;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall RoutingTable::HasEndpoint(
        _in const std::string& szPath,
        _in HttpMethod eMethod
    ) const
    {
        bool bResult = false;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        std::string szRouteKey = HttpMethodToString(eMethod) + ":" + szPath;
        if (m_stdszun32RouteIndex.find(szRouteKey) != m_stdszun32RouteIndex.end())
        {
            bResult = true;
        }
        else
        {
            // Check pattern matching
            for (const auto& sEndpoint : m_stdsEndpoints)
            {
                if (sEndpoint.m_eMethod == eMethod)
                {
                    std::unordered_map<std::string, std::string> stdszszDummy;
                    if (sEndpoint.MatchesPath(szPath, stdszszDummy))
                    {
                        bResult = true;
                        break;
                    }
                }
            }
        }

        return bResult;
    }

    uint32_t __thiscall RoutingTable::GetEndpointCount() const
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);
        uint32_t un32Result = static_cast<uint32_t>(m_stdsEndpoints.size());
        return un32Result;
    }

    std::vector<EndpointDefinition> __thiscall RoutingTable::GetAllEndpoints() const
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);
        return m_stdsEndpoints;
    }

    std::vector<EndpointDefinition> __thiscall RoutingTable::GetEndpointsByBackend(
        _in const std::string& szBackendIdentifier
    ) const
    {
        std::vector<EndpointDefinition> stdsResult;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        for (const auto& sEndpoint : m_stdsEndpoints)
        {
            if (sEndpoint.m_szBackendIdentifier == szBackendIdentifier)
            {
                stdsResult.push_back(sEndpoint);
            }
        }

        return stdsResult;
    }

    uint32_t __thiscall RoutingTable::RemoveEndpointsByBackend(
        _in const std::string& szBackendIdentifier
    )
    {
        uint32_t un32RemovedCount = 0;

        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);

        // Rebuild the endpoint list and index, excluding the target backend
        std::vector<EndpointDefinition> stdsNewEndpoints;
        std::unordered_map<std::string, uint32_t> stdszun32NewIndex;

        for (const auto& sEndpoint : m_stdsEndpoints)
        {
            if (sEndpoint.m_szBackendIdentifier != szBackendIdentifier)
            {
                uint32_t un32NewIndex = static_cast<uint32_t>(stdsNewEndpoints.size());
                stdsNewEndpoints.push_back(sEndpoint);
                stdszun32NewIndex[sEndpoint.GetRouteKey()] = un32NewIndex;
            }
            else
            {
                ++un32RemovedCount;
            }
        }

        m_stdsEndpoints = std::move(stdsNewEndpoints);
        m_stdszun32RouteIndex = std::move(stdszun32NewIndex);

        if (un32RemovedCount > 0)
        {
            GATEWAY_LOG_INFO("RoutingTable",
                "Removed " + std::to_string(un32RemovedCount) +
                " endpoints for backend: " + szBackendIdentifier);
        }

        return un32RemovedCount;
    }

    void __thiscall RoutingTable::Clear()
    {
        std::lock_guard<std::mutex> stdLock(m_stdMutexAccess);
        m_stdsEndpoints.clear();
        m_stdszun32RouteIndex.clear();
    }

} // namespace Gateway
