#pragma once

#include "gateway/Common.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>
#include <memory>

namespace Gateway
{

    enum class JsonValueType
    {
        Null,
        Boolean,
        Integer,
        Float,
        String,
        Array,
        Object
    };

    class JsonValue
    {
        public:
            JsonValue();
            ~JsonValue() = default;

            static JsonValue __stdcall CreateNull();
            static JsonValue __stdcall CreateBoolean(
                _in bool bValue
            );
            static JsonValue __stdcall CreateInteger(
                _in int64_t n64Value
            );
            static JsonValue __stdcall CreateFloat(
                _in double fl64Value
            );
            static JsonValue __stdcall CreateString(
                _in const std::string& szValue
            );
            static JsonValue __stdcall CreateArray();
            static JsonValue __stdcall CreateObject();

            JsonValueType __thiscall GetType() const;
            bool __thiscall IsNull() const;
            bool __thiscall IsBoolean() const;
            bool __thiscall IsInteger() const;
            bool __thiscall IsFloat() const;
            bool __thiscall IsString() const;
            bool __thiscall IsArray() const;
            bool __thiscall IsObject() const;

            bool __thiscall GetBoolean() const;
            int64_t __thiscall GetInteger() const;
            double __thiscall GetFloat() const;
            std::string __thiscall GetString() const;

            uint32_t __thiscall GetArraySize() const;
            JsonValue __thiscall GetArrayElement(
                _in uint32_t un32Index
            ) const;
            void __thiscall AddArrayElement(
                _in const JsonValue& sValue
            );

            bool __thiscall HasMember(
                _in const std::string& szKey
            ) const;
            JsonValue __thiscall GetMember(
                _in const std::string& szKey
            ) const;
            void __thiscall SetMember(
                _in const std::string& szKey,
                _in const JsonValue& sValue
            );
            std::vector<std::string> __thiscall GetMemberKeys() const;

            std::string __thiscall Serialize() const;

        private:
            JsonValueType                                       m_eType;
            bool                                                m_bBooleanValue;
            int64_t                                             m_n64IntegerValue;
            double                                              m_fl64FloatValue;
            std::string                                         m_szStringValue;
            std::vector<std::shared_ptr<JsonValue>>             m_stdspArrayElements;
            std::vector<std::pair<std::string, std::shared_ptr<JsonValue>>>  m_stdspObjectMembers;

            void __thiscall SerializeString(
                _in const std::string& szInput,
                _out std::string& szOutput
            ) const;
    };

    class JsonParser
    {
        public:
            JsonParser();
            ~JsonParser() = default;

            bool __thiscall Parse(
                _in const std::string& szJsonText,
                _out JsonValue& sValue
            );

            std::string __thiscall GetErrorMessage() const;
            uint32_t __thiscall GetErrorPosition() const;

        private:
            bool __thiscall ParseValue(
                _out JsonValue& sValue
            );
            bool __thiscall ParseNull(
                _out JsonValue& sValue
            );
            bool __thiscall ParseBoolean(
                _out JsonValue& sValue
            );
            bool __thiscall ParseNumber(
                _out JsonValue& sValue
            );
            bool __thiscall ParseString(
                _out std::string& szValue
            );
            bool __thiscall ParseStringValue(
                _out JsonValue& sValue
            );
            bool __thiscall ParseArray(
                _out JsonValue& sValue
            );
            bool __thiscall ParseObject(
                _out JsonValue& sValue
            );

            void __thiscall SkipWhitespace();
            char __thiscall CurrentChar() const;
            char __thiscall NextChar();
            bool __thiscall HasMore() const;
            void __thiscall SetError(
                _in const std::string& szMessage
            );

            std::string         m_szInput;
            uint32_t            m_un32Position;
            std::string         m_szErrorMessage;
            uint32_t            m_un32ErrorPosition;
    };

    std::string __stdcall SerializeJsonObject(
        _in const std::unordered_map<std::string, std::string>& stdszszMembers
    );

} // namespace Gateway
