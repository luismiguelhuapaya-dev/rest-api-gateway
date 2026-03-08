#include "gateway/validation/JsonParser.h"
#include <sstream>
#include <cstring>
#include <iomanip>

namespace Gateway
{

    // JsonValue implementation
    JsonValue::JsonValue()
        : m_eType(JsonValueType::Null)
        , m_bBooleanValue(false)
        , m_n64IntegerValue(0)
        , m_fl64FloatValue(0.0)
    {
    }

    JsonValue __stdcall JsonValue::CreateNull()
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Null;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateBoolean(
        _in bool bValue
    )
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Boolean;
        sValue.m_bBooleanValue = bValue;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateInteger(
        _in int64_t n64Value
    )
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Integer;
        sValue.m_n64IntegerValue = n64Value;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateFloat(
        _in double fl64Value
    )
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Float;
        sValue.m_fl64FloatValue = fl64Value;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateString(
        _in const std::string& szValue
    )
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::String;
        sValue.m_szStringValue = szValue;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateArray()
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Array;
        return sValue;
    }

    JsonValue __stdcall JsonValue::CreateObject()
    {
        JsonValue sValue;
        sValue.m_eType = JsonValueType::Object;
        return sValue;
    }

    JsonValueType __thiscall JsonValue::GetType() const
    {
        return m_eType;
    }

    bool __thiscall JsonValue::IsNull() const
    {
        return (m_eType == JsonValueType::Null);
    }

    bool __thiscall JsonValue::IsBoolean() const
    {
        return (m_eType == JsonValueType::Boolean);
    }

    bool __thiscall JsonValue::IsInteger() const
    {
        return (m_eType == JsonValueType::Integer);
    }

    bool __thiscall JsonValue::IsFloat() const
    {
        return (m_eType == JsonValueType::Float);
    }

    bool __thiscall JsonValue::IsString() const
    {
        return (m_eType == JsonValueType::String);
    }

    bool __thiscall JsonValue::IsArray() const
    {
        return (m_eType == JsonValueType::Array);
    }

    bool __thiscall JsonValue::IsObject() const
    {
        return (m_eType == JsonValueType::Object);
    }

    bool __thiscall JsonValue::GetBoolean() const
    {
        return m_bBooleanValue;
    }

    int64_t __thiscall JsonValue::GetInteger() const
    {
        return m_n64IntegerValue;
    }

    double __thiscall JsonValue::GetFloat() const
    {
        return m_fl64FloatValue;
    }

    std::string __thiscall JsonValue::GetString() const
    {
        return m_szStringValue;
    }

    uint32_t __thiscall JsonValue::GetArraySize() const
    {
        uint32_t un32Result = 0;
        if (m_eType == JsonValueType::Array)
        {
            un32Result = static_cast<uint32_t>(m_stdspArrayElements.size());
        }
        return un32Result;
    }

    JsonValue __thiscall JsonValue::GetArrayElement(
        _in uint32_t un32Index
    ) const
    {
        JsonValue sResult;
        if ((m_eType == JsonValueType::Array) && (un32Index < m_stdspArrayElements.size()))
        {
            sResult = *m_stdspArrayElements[un32Index];
        }
        return sResult;
    }

    void __thiscall JsonValue::AddArrayElement(
        _in const JsonValue& sValue
    )
    {
        if (m_eType == JsonValueType::Array)
        {
            m_stdspArrayElements.push_back(std::make_shared<JsonValue>(sValue));
        }
    }

    bool __thiscall JsonValue::HasMember(
        _in const std::string& szKey
    ) const
    {
        bool bResult = false;
        if (m_eType == JsonValueType::Object)
        {
            for (const auto& stdPair : m_stdspObjectMembers)
            {
                if (stdPair.first == szKey)
                {
                    bResult = true;
                    break;
                }
            }
        }
        return bResult;
    }

    JsonValue __thiscall JsonValue::GetMember(
        _in const std::string& szKey
    ) const
    {
        JsonValue sResult;
        if (m_eType == JsonValueType::Object)
        {
            for (const auto& stdPair : m_stdspObjectMembers)
            {
                if (stdPair.first == szKey)
                {
                    sResult = *stdPair.second;
                    break;
                }
            }
        }
        return sResult;
    }

    void __thiscall JsonValue::SetMember(
        _in const std::string& szKey,
        _in const JsonValue& sValue
    )
    {
        if (m_eType == JsonValueType::Object)
        {
            // Check if key already exists
            for (auto& stdPair : m_stdspObjectMembers)
            {
                if (stdPair.first == szKey)
                {
                    *stdPair.second = sValue;
                    return;
                }
            }
            m_stdspObjectMembers.push_back(
                std::make_pair(szKey, std::make_shared<JsonValue>(sValue))
            );
        }
    }

    std::vector<std::string> __thiscall JsonValue::GetMemberKeys() const
    {
        std::vector<std::string> stdszResult;
        if (m_eType == JsonValueType::Object)
        {
            for (const auto& stdPair : m_stdspObjectMembers)
            {
                stdszResult.push_back(stdPair.first);
            }
        }
        return stdszResult;
    }

    std::string __thiscall JsonValue::Serialize() const
    {
        std::string szResult;

        if (m_eType == JsonValueType::Null)
        {
            szResult = "null";
        }
        else if (m_eType == JsonValueType::Boolean)
        {
            szResult = m_bBooleanValue ? "true" : "false";
        }
        else if (m_eType == JsonValueType::Integer)
        {
            szResult = std::to_string(m_n64IntegerValue);
        }
        else if (m_eType == JsonValueType::Float)
        {
            std::ostringstream stdStream;
            stdStream << m_fl64FloatValue;
            szResult = stdStream.str();
            // Ensure there is a decimal point
            if ((szResult.find('.') == std::string::npos) &&
                (szResult.find('e') == std::string::npos) &&
                (szResult.find('E') == std::string::npos))
            {
                szResult += ".0";
            }
        }
        else if (m_eType == JsonValueType::String)
        {
            SerializeString(m_szStringValue, szResult);
        }
        else if (m_eType == JsonValueType::Array)
        {
            szResult = "[";
            for (size_t un64Index = 0; (un64Index < m_stdspArrayElements.size()); ++un64Index)
            {
                if (un64Index > 0)
                {
                    szResult += ",";
                }
                szResult += m_stdspArrayElements[un64Index]->Serialize();
            }
            szResult += "]";
        }
        else if (m_eType == JsonValueType::Object)
        {
            szResult = "{";
            for (size_t un64Index = 0; (un64Index < m_stdspObjectMembers.size()); ++un64Index)
            {
                if (un64Index > 0)
                {
                    szResult += ",";
                }
                std::string szKey;
                SerializeString(m_stdspObjectMembers[un64Index].first, szKey);
                szResult += szKey + ":" + m_stdspObjectMembers[un64Index].second->Serialize();
            }
            szResult += "}";
        }

        return szResult;
    }

    void __thiscall JsonValue::SerializeString(
        _in const std::string& szInput,
        _out std::string& szOutput
    ) const
    {
        szOutput = "\"";
        for (char chCharacter : szInput)
        {
            if (chCharacter == '"')
            {
                szOutput += "\\\"";
            }
            else if (chCharacter == '\\')
            {
                szOutput += "\\\\";
            }
            else if (chCharacter == '\b')
            {
                szOutput += "\\b";
            }
            else if (chCharacter == '\f')
            {
                szOutput += "\\f";
            }
            else if (chCharacter == '\n')
            {
                szOutput += "\\n";
            }
            else if (chCharacter == '\r')
            {
                szOutput += "\\r";
            }
            else if (chCharacter == '\t')
            {
                szOutput += "\\t";
            }
            else if (static_cast<unsigned char>(chCharacter) < 0x20)
            {
                std::ostringstream stdStream;
                stdStream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                          << static_cast<int>(static_cast<unsigned char>(chCharacter));
                szOutput += stdStream.str();
            }
            else
            {
                szOutput += chCharacter;
            }
        }
        szOutput += "\"";
    }

    // JsonParser implementation
    JsonParser::JsonParser()
        : m_un32Position(0)
        , m_un32ErrorPosition(0)
    {
    }

    bool __thiscall JsonParser::Parse(
        _in const std::string& szJsonText,
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        m_szInput = szJsonText;
        m_un32Position = 0;
        m_szErrorMessage.clear();
        m_un32ErrorPosition = 0;

        SkipWhitespace();
        if (HasMore())
        {
            if (ParseValue(sValue))
            {
                SkipWhitespace();
                bResult = true;
            }
        }
        else
        {
            SetError("Empty input");
        }

        return bResult;
    }

    std::string __thiscall JsonParser::GetErrorMessage() const
    {
        return m_szErrorMessage;
    }

    uint32_t __thiscall JsonParser::GetErrorPosition() const
    {
        return m_un32ErrorPosition;
    }

    bool __thiscall JsonParser::ParseValue(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        SkipWhitespace();
        if (HasMore())
        {
            char chCurrent = CurrentChar();
            if (chCurrent == 'n')
            {
                bResult = ParseNull(sValue);
            }
            else if ((chCurrent == 't') || (chCurrent == 'f'))
            {
                bResult = ParseBoolean(sValue);
            }
            else if ((chCurrent == '-') || ((chCurrent >= '0') && (chCurrent <= '9')))
            {
                bResult = ParseNumber(sValue);
            }
            else if (chCurrent == '"')
            {
                bResult = ParseStringValue(sValue);
            }
            else if (chCurrent == '[')
            {
                bResult = ParseArray(sValue);
            }
            else if (chCurrent == '{')
            {
                bResult = ParseObject(sValue);
            }
            else
            {
                SetError("Unexpected character");
            }
        }
        else
        {
            SetError("Unexpected end of input");
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseNull(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        if (((m_un32Position + 4) <= m_szInput.size()) &&
            (m_szInput.substr(m_un32Position, 4) == "null"))
        {
            sValue = JsonValue::CreateNull();
            m_un32Position += 4;
            bResult = true;
        }
        else
        {
            SetError("Invalid null value");
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseBoolean(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        if (((m_un32Position + 4) <= m_szInput.size()) &&
            (m_szInput.substr(m_un32Position, 4) == "true"))
        {
            sValue = JsonValue::CreateBoolean(true);
            m_un32Position += 4;
            bResult = true;
        }
        else if (((m_un32Position + 5) <= m_szInput.size()) &&
                 (m_szInput.substr(m_un32Position, 5) == "false"))
        {
            sValue = JsonValue::CreateBoolean(false);
            m_un32Position += 5;
            bResult = true;
        }
        else
        {
            SetError("Invalid boolean value");
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseNumber(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        uint32_t un32Start = m_un32Position;
        bool bIsFloat = false;

        if ((HasMore()) && (CurrentChar() == '-'))
        {
            NextChar();
        }

        while ((HasMore()) && (CurrentChar() >= '0') && (CurrentChar() <= '9'))
        {
            NextChar();
        }

        if ((HasMore()) && (CurrentChar() == '.'))
        {
            bIsFloat = true;
            NextChar();
            while ((HasMore()) && (CurrentChar() >= '0') && (CurrentChar() <= '9'))
            {
                NextChar();
            }
        }

        if ((HasMore()) && ((CurrentChar() == 'e') || (CurrentChar() == 'E')))
        {
            bIsFloat = true;
            NextChar();
            if ((HasMore()) && ((CurrentChar() == '+') || (CurrentChar() == '-')))
            {
                NextChar();
            }
            while ((HasMore()) && (CurrentChar() >= '0') && (CurrentChar() <= '9'))
            {
                NextChar();
            }
        }

        if (m_un32Position > un32Start)
        {
            std::string szNumberString = m_szInput.substr(un32Start, m_un32Position - un32Start);

            if (bIsFloat)
            {
                double fl64Value = std::stod(szNumberString);
                sValue = JsonValue::CreateFloat(fl64Value);
            }
            else
            {
                int64_t n64Value = std::stoll(szNumberString);
                sValue = JsonValue::CreateInteger(n64Value);
            }
            bResult = true;
        }
        else
        {
            SetError("Invalid number");
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseString(
        _out std::string& szValue
    )
    {
        bool bResult = false;

        if ((HasMore()) && (CurrentChar() == '"'))
        {
            NextChar(); // Skip opening quote
            szValue.clear();
            bResult = true;

            while ((HasMore()) && (CurrentChar() != '"') && (bResult))
            {
                if (CurrentChar() == '\\')
                {
                    NextChar();
                    if (HasMore())
                    {
                        char chEscaped = CurrentChar();
                        if (chEscaped == '"')
                        {
                            szValue += '"';
                        }
                        else if (chEscaped == '\\')
                        {
                            szValue += '\\';
                        }
                        else if (chEscaped == '/')
                        {
                            szValue += '/';
                        }
                        else if (chEscaped == 'b')
                        {
                            szValue += '\b';
                        }
                        else if (chEscaped == 'f')
                        {
                            szValue += '\f';
                        }
                        else if (chEscaped == 'n')
                        {
                            szValue += '\n';
                        }
                        else if (chEscaped == 'r')
                        {
                            szValue += '\r';
                        }
                        else if (chEscaped == 't')
                        {
                            szValue += '\t';
                        }
                        else if (chEscaped == 'u')
                        {
                            // Unicode escape: \uXXXX
                            if ((m_un32Position + 4) < m_szInput.size())
                            {
                                std::string szHex = m_szInput.substr(m_un32Position + 1, 4);
                                uint32_t un32CodePoint = static_cast<uint32_t>(std::stoul(szHex, nullptr, 16));

                                if (un32CodePoint < 0x80)
                                {
                                    szValue += static_cast<char>(un32CodePoint);
                                }
                                else if (un32CodePoint < 0x800)
                                {
                                    szValue += static_cast<char>(0xC0 | ((un32CodePoint >> 6) & 0x1F));
                                    szValue += static_cast<char>(0x80 | (un32CodePoint & 0x3F));
                                }
                                else
                                {
                                    szValue += static_cast<char>(0xE0 | ((un32CodePoint >> 12) & 0x0F));
                                    szValue += static_cast<char>(0x80 | ((un32CodePoint >> 6) & 0x3F));
                                    szValue += static_cast<char>(0x80 | (un32CodePoint & 0x3F));
                                }

                                m_un32Position += 4;
                            }
                            else
                            {
                                SetError("Invalid unicode escape");
                                bResult = false;
                            }
                        }
                        else
                        {
                            SetError("Invalid escape character");
                            bResult = false;
                        }
                        NextChar();
                    }
                    else
                    {
                        SetError("Unexpected end of string");
                        bResult = false;
                    }
                }
                else
                {
                    szValue += CurrentChar();
                    NextChar();
                }
            }

            if ((bResult) && (HasMore()) && (CurrentChar() == '"'))
            {
                NextChar(); // Skip closing quote
            }
            else if (bResult)
            {
                SetError("Unterminated string");
                bResult = false;
            }
        }
        else
        {
            SetError("Expected string");
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseStringValue(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        std::string szParsedString;
        if (ParseString(szParsedString))
        {
            sValue = JsonValue::CreateString(szParsedString);
            bResult = true;
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseArray(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        if ((HasMore()) && (CurrentChar() == '['))
        {
            NextChar(); // Skip '['
            sValue = JsonValue::CreateArray();
            SkipWhitespace();

            if ((HasMore()) && (CurrentChar() == ']'))
            {
                NextChar(); // Empty array
                bResult = true;
            }
            else
            {
                bResult = true;
                while ((bResult) && (HasMore()))
                {
                    SkipWhitespace();
                    JsonValue sElement;
                    if (ParseValue(sElement))
                    {
                        sValue.AddArrayElement(sElement);
                        SkipWhitespace();

                        if ((HasMore()) && (CurrentChar() == ','))
                        {
                            NextChar();
                        }
                        else if ((HasMore()) && (CurrentChar() == ']'))
                        {
                            NextChar();
                            break;
                        }
                        else
                        {
                            SetError("Expected ',' or ']' in array");
                            bResult = false;
                        }
                    }
                    else
                    {
                        bResult = false;
                    }
                }
            }
        }

        return bResult;
    }

    bool __thiscall JsonParser::ParseObject(
        _out JsonValue& sValue
    )
    {
        bool bResult = false;

        if ((HasMore()) && (CurrentChar() == '{'))
        {
            NextChar(); // Skip '{'
            sValue = JsonValue::CreateObject();
            SkipWhitespace();

            if ((HasMore()) && (CurrentChar() == '}'))
            {
                NextChar(); // Empty object
                bResult = true;
            }
            else
            {
                bResult = true;
                while ((bResult) && (HasMore()))
                {
                    SkipWhitespace();

                    std::string szKey;
                    if (ParseString(szKey))
                    {
                        SkipWhitespace();
                        if ((HasMore()) && (CurrentChar() == ':'))
                        {
                            NextChar();
                            SkipWhitespace();

                            JsonValue sMemberValue;
                            if (ParseValue(sMemberValue))
                            {
                                sValue.SetMember(szKey, sMemberValue);
                                SkipWhitespace();

                                if ((HasMore()) && (CurrentChar() == ','))
                                {
                                    NextChar();
                                }
                                else if ((HasMore()) && (CurrentChar() == '}'))
                                {
                                    NextChar();
                                    break;
                                }
                                else
                                {
                                    SetError("Expected ',' or '}' in object");
                                    bResult = false;
                                }
                            }
                            else
                            {
                                bResult = false;
                            }
                        }
                        else
                        {
                            SetError("Expected ':' after object key");
                            bResult = false;
                        }
                    }
                    else
                    {
                        bResult = false;
                    }
                }
            }
        }

        return bResult;
    }

    void __thiscall JsonParser::SkipWhitespace()
    {
        while ((HasMore()) &&
               ((CurrentChar() == ' ') || (CurrentChar() == '\t') ||
                (CurrentChar() == '\n') || (CurrentChar() == '\r')))
        {
            NextChar();
        }
    }

    char __thiscall JsonParser::CurrentChar() const
    {
        char chResult = '\0';
        if (m_un32Position < m_szInput.size())
        {
            chResult = m_szInput[m_un32Position];
        }
        return chResult;
    }

    char __thiscall JsonParser::NextChar()
    {
        ++m_un32Position;
        char chResult = '\0';
        if (m_un32Position < m_szInput.size())
        {
            chResult = m_szInput[m_un32Position];
        }
        return chResult;
    }

    bool __thiscall JsonParser::HasMore() const
    {
        return (m_un32Position < m_szInput.size());
    }

    void __thiscall JsonParser::SetError(
        _in const std::string& szMessage
    )
    {
        m_szErrorMessage = szMessage;
        m_un32ErrorPosition = m_un32Position;
    }

    std::string __stdcall SerializeJsonObject(
        _in const std::unordered_map<std::string, std::string>& stdszszMembers
    )
    {
        std::string szResult = "{";
        bool bFirst = true;

        for (const auto& stdPair : stdszszMembers)
        {
            if (!bFirst)
            {
                szResult += ",";
            }
            bFirst = false;

            szResult += "\"";
            for (char chCharacter : stdPair.first)
            {
                if (chCharacter == '"')
                {
                    szResult += "\\\"";
                }
                else if (chCharacter == '\\')
                {
                    szResult += "\\\\";
                }
                else
                {
                    szResult += chCharacter;
                }
            }
            szResult += "\":\"";
            for (char chCharacter : stdPair.second)
            {
                if (chCharacter == '"')
                {
                    szResult += "\\\"";
                }
                else if (chCharacter == '\\')
                {
                    szResult += "\\\\";
                }
                else
                {
                    szResult += chCharacter;
                }
            }
            szResult += "\"";
        }

        szResult += "}";
        return szResult;
    }

} // namespace Gateway
