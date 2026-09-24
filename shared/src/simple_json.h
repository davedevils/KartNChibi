
#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>

namespace SimpleJSON {

class Value {
public:
    enum Type { NULL_TYPE, BOOL, NUMBER, STRING, ARRAY, OBJECT };
    
    Value() : type(NULL_TYPE), numValue(0.0), boolValue(false) {}
    explicit Value(bool b) : type(BOOL), numValue(0.0), boolValue(b) {}
    explicit Value(double n) : type(NUMBER), numValue(n), boolValue(false) {}
    explicit Value(const std::string& s) : type(STRING), strValue(s), numValue(0.0), boolValue(false) {}
    
    Type GetType() const { return type; }
    
    bool AsBool() const { return boolValue; }
    int AsInt() const { return (int)numValue; }
    double AsDouble() const { return numValue; }
    const std::string& AsString() const { return strValue; }
    
    size_t ArraySize() const { return arrayValue.size(); }
    const Value& operator[](size_t index) const {
        static Value null_value;
        return (index < arrayValue.size()) ? arrayValue[index] : null_value;
    }
    
    bool HasKey(const std::string& key) const {
        return objectValue.find(key) != objectValue.end();
    }
    
    const Value& operator[](const std::string& key) const {
        static Value null_value;
        auto it = objectValue.find(key);
        return (it != objectValue.end()) ? it->second : null_value;
    }
    
    void SetBool(bool b) { type = BOOL; boolValue = b; }
    void SetNumber(double n) { type = NUMBER; numValue = n; }
    void SetString(const std::string& s) { type = STRING; strValue = s; }
    void AddArrayElement(const Value& v) {
        if (type != ARRAY) { type = ARRAY; arrayValue.clear(); }
        arrayValue.push_back(v);
    }
    void SetObjectKey(const std::string& key, const Value& v) {
        if (type != OBJECT) { type = OBJECT; objectValue.clear(); }
        objectValue[key] = v;
    }
    
private:
    Type type;
    bool boolValue;
    double numValue;
    std::string strValue;
    std::vector<Value> arrayValue;
    std::map<std::string, Value> objectValue;
};

class Parser {
public:
    static Value Parse(const std::string& json) {
        Parser p(json);
        return p.ParseValue();
    }
    
    static Value ParseFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return Value();
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Parse(buffer.str());
    }
    
private:
    Parser(const std::string& json) : json(json), pos(0) {}
    
    char Peek() {
        SkipWhitespace();
        return (pos < json.size()) ? json[pos] : '\0';
    }
    
    char Get() {
        SkipWhitespace();
        return (pos < json.size()) ? json[pos++] : '\0';
    }
    
    void SkipWhitespace() {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
               json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }
    
    Value ParseValue() {
        char c = Peek();
        
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber();
        
        return Value();
    }
    
    Value ParseObject() {
        Value obj;
        Get();

        while (Peek() != '}' && Peek() != '\0') {
            if (Peek() != '"') break;
            Value keyVal = ParseString();
            std::string key = keyVal.AsString();

            if (Get() != ':') break;

            Value value = ParseValue();
            obj.SetObjectKey(key, value);

            if (Peek() == ',') Get();
        }

        Get();
        return obj;
    }
    
    Value ParseArray() {
        Value arr;
        Get();

        while (Peek() != ']' && Peek() != '\0') {
            Value element = ParseValue();
            arr.AddArrayElement(element);

            if (Peek() == ',') Get();
        }

        Get();
        return arr;
    }
    
    Value ParseString() {
        Get();
        std::string str;

        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                char c = json[pos++];
                if (c == 'n') str += '\n';
                else if (c == 't') str += '\t';
                else if (c == 'r') str += '\r';
                else str += c;
            } else {
                str += json[pos++];
            }
        }

        Get();
        return Value(str);
    }
    
    Value ParseNumber() {
        size_t start = pos;
        
        if (json[pos] == '-') pos++;
        
        while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || 
               json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || 
               json[pos] == '+' || json[pos] == '-')) {
            pos++;
        }
        
        std::string numStr = json.substr(start, pos - start);
        return Value(std::stod(numStr));
    }
    
    Value ParseBool() {
        if (json.substr(pos, 4) == "true") {
            pos += 4;
            return Value(true);
        }
        if (json.substr(pos, 5) == "false") {
            pos += 5;
            return Value(false);
        }
        return Value();
    }
    
    Value ParseNull() {
        if (json.substr(pos, 4) == "null") {
            pos += 4;
        }
        return Value();
    }
    
    std::string json;
    size_t pos;
};

}

