/*********************************************************************************
* @File			ReflectionBase.cpp
* @Author		Soh Wei Jie, weijie.soh@digipen.edu
* @Co-Author	-
* @Date			26/9/2025
* @Brief        Implements the engine's reflection and persistence building blocks:
*                   - TypeDescriptor base and concrete descriptors for user structs, STL containers
*                     (std::vector, std::unordered_map, std::pair), std::shared_ptr, and a GUID_128 type.
*                   - TypeResolver machinery to obtain descriptors for reflected and primitive types,
*                     with lazy resolution support for templated containers.
*                   - JSON serialization/deserialization designed around rapidjson (Writer + Document),
*                     producing compact JSON for storage and network interchange.
*                   - Thread-safe type registry protected by a mutex; registration macros
*                     (REFL_SERIALIZABLE / REFL_REGISTER_START / REFL_REGISTER_PROPERTY / REFL_REGISTER_END)
*                     to declare reflection for user types.
*                   - Portable persistence helpers (little-endian conversion), alignment macro, and
*                     Base64 encoding/decoding used to serialize opaque shared_ptr<void> blobs (expects
*                     a size-prefix contract).
*                   - Mobile-friendly behavior: avoids global namespace pollution, throws descriptive
*                     exceptions for malformed input or resolution failures (rather than asserts),
*                     and guards descriptor registration to be safe on resource-constrained platforms.
*                   Usage notes: user types must call InitReflection via the provided macros to register
*                   members; TypeResolver will return primitive descriptors for non-reflected types.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#include "pch.h"
#include "Reflection/ReflectionBase.hpp"

// Helper: read a rapidjson::Value into a C++ type T using typed getters.
// CHANGE: rapidjson does not provide a templated Get<T>() for arbitrary types; we centralize conversions here.
#pragma region READJSON
template <typename T>
T ReadJsonAs(const rapidjson::Value& v);

template <>
inline bool ReadJsonAs<bool>(const rapidjson::Value& v) { return v.GetBool(); }

template <>
inline int ReadJsonAs<int>(const rapidjson::Value& v)
{
    if (v.IsInt()) return v.GetInt();
    if (v.IsInt64()) return static_cast<int>(v.GetInt64());
    if (v.IsDouble()) return static_cast<int>(v.GetDouble());
    throw std::runtime_error("JSON value is not an integer (int)");
}

template <>
inline unsigned ReadJsonAs<unsigned>(const rapidjson::Value& v)
{
    if (v.IsUint()) return v.GetUint();
    if (v.IsUint64()) return static_cast<unsigned>(v.GetUint64());
    if (v.IsDouble()) return static_cast<unsigned>(v.GetDouble());
    throw std::runtime_error("JSON value is not an unsigned integer (unsigned)");
}

template <>
inline int64_t ReadJsonAs<int64_t>(const rapidjson::Value& v)
{
    if (v.IsInt64()) return v.GetInt64();
    if (v.IsInt()) return static_cast<int64_t>(v.GetInt());
    if (v.IsDouble()) return static_cast<int64_t>(v.GetDouble());
    throw std::runtime_error("JSON value is not an integer (int64_t)");
}

template <>
inline uint64_t ReadJsonAs<uint64_t>(const rapidjson::Value& v)
{
    if (v.IsUint64()) return v.GetUint64();
    if (v.IsUint()) return static_cast<uint64_t>(v.GetUint());
    if (v.IsDouble()) return static_cast<uint64_t>(v.GetDouble());
    throw std::runtime_error("JSON value is not an unsigned integer (uint64_t)");
}

template <>
inline double ReadJsonAs<double>(const rapidjson::Value& v)
{
    if (v.IsDouble()) return v.GetDouble();
    if (v.IsInt()) return static_cast<double>(v.GetInt());
    if (v.IsInt64()) return static_cast<double>(v.GetInt64());
    throw std::runtime_error("JSON value is not a number (double)");
}

template <>
inline float ReadJsonAs<float>(const rapidjson::Value& v)
{
    return static_cast<float>(ReadJsonAs<double>(v));
}

template <>
inline std::string ReadJsonAs<std::string>(const rapidjson::Value& v)
{
    if (!v.IsString()) throw std::runtime_error("JSON value is not a string");
    return std::string(v.GetString(), v.GetStringLength());
}
#pragma endregion

#pragma region Macros
// -------------------------------
// TypeDescriptor :: Impl (definition lives here)
struct TypeDescriptor::Impl
{
    std::string name;
    size_t size = 0;
    Impl() = default;
    Impl(const std::string& n, size_t s) : name(n), size(s) {}
};

// ctor / dtor / accessors
TypeDescriptor::TypeDescriptor(const std::string& name_, size_t size_)
    : pImpl(new Impl(name_, size_))
{
}

TypeDescriptor::~TypeDescriptor()
{
    delete pImpl;
    pImpl = nullptr;
}

const char* TypeDescriptor::GetName() const
{
    return (pImpl && !pImpl->name.empty()) ? pImpl->name.c_str() : nullptr;
}

void TypeDescriptor::SetName(const char* name)
{
    if (!pImpl) pImpl = new Impl;
    pImpl->name = (name ? name : "");
}

size_t TypeDescriptor::GetSize() const
{
    return pImpl ? pImpl->size : 0;
}

void TypeDescriptor::SetSize(size_t s)
{
    if (!pImpl) pImpl = new Impl;
    pImpl->size = s;
}

void TypeDescriptor::SerializeJson(const void* obj, rapidjson::Document& out) const
{
    // Default fallback: use textual Serialize -> parse into Document
    // This keeps compatibility with existing code paths that expect a Document.
    std::stringstream ss;
    Serialize(obj, ss);
    out.Parse(ss.str().c_str());
}

// Registry helpers
std::unordered_map<std::string, TypeDescriptor*>& TypeDescriptor::type_descriptor_lookup()
{
    static std::unordered_map<std::string, TypeDescriptor*> s_map;
    return s_map;
}

std::mutex& TypeDescriptor::descriptor_registry_mutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

// -------------------------------
// TypeDescriptor_Struct impl storage in .cpp
// We store the actual std::vector<Member> inside this Impl, and keep a void* pointer
// in the header (struct_impl) which we cast to TypeDescriptor_Struct_Impl* here.
struct TypeDescriptor_Struct_Impl
{
    std::vector<TypeDescriptor_Struct::Member> members;
};

static inline TypeDescriptor_Struct_Impl* SImpl(TypeDescriptor_Struct* s)
{
    return reinterpret_cast<TypeDescriptor_Struct_Impl*>(s->struct_impl);
}
static inline const TypeDescriptor_Struct_Impl* SImplConst(const TypeDescriptor_Struct* s)
{
    return reinterpret_cast<const TypeDescriptor_Struct_Impl*>(s->struct_impl);
}

// ctor/dtor and member accessors
TypeDescriptor_Struct::TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*))
    : TypeDescriptor("", 0)
    , struct_impl(nullptr)
{
    // call init to allow macros to call SetName/SetSize/SetMembers
    if (init) init(this);
}

TypeDescriptor_Struct::~TypeDescriptor_Struct()
{
    if (struct_impl)
    {
        delete SImpl(this);
        struct_impl = nullptr;
    }
}

void TypeDescriptor_Struct::SetMembers(const std::initializer_list<Member>& members)
{
    if (!struct_impl) struct_impl = new TypeDescriptor_Struct_Impl();
    TypeDescriptor_Struct_Impl* impl = SImpl(this);
    impl->members.assign(members.begin(), members.end());
}

std::vector<TypeDescriptor_Struct::Member> TypeDescriptor_Struct::GetMembers() const
{
    const TypeDescriptor_Struct_Impl* impl = SImplConst(this);
    if (!impl) return {};
    return impl->members; // copy
}

void TypeDescriptor_Struct::Dump(const void* obj, std::ostream& os, int indent_level) const
{
    os << (GetName() ? GetName() : "") << "\n" << std::string(4 * indent_level, ' ') << "{\n";
    const TypeDescriptor_Struct_Impl* impl = SImplConst(this);
    if (!impl) { os << std::string(4 * indent_level, ' ') << "}\n"; return; }

    for (const Member& member : impl->members)
    {
        os << std::string(4 * (indent_level + 1), ' ') << member.name << " = ";
        void* member_addr = member.get_ptr(const_cast<void*>(obj));
        member.type->Dump(member_addr, os, indent_level + 1);
        os << "\n";
    }
    os << std::string(4 * indent_level, ' ') << "}\n";
}

void TypeDescriptor_Struct::Serialize(const void* obj, std::ostream& os) const
{
    const TypeDescriptor_Struct_Impl* impl = SImplConst(this);
    os << R"({"type":")" << (GetName() ? GetName() : "") << R"(","data":[)";
    if (!impl) { os << "]}"; return; }

    bool first = true;
    for (const Member& member : impl->members)
    {
        if (!first) os << ",";
        first = false;
        void* member_addr = member.get_ptr(const_cast<void*>(obj));
        member.type->Serialize(member_addr, os);
    }
    os << "]}";
}

void TypeDescriptor_Struct::Deserialize(void* obj, const rapidjson::Value& value) const
{
    // Validate target object
    if (!obj)
    {
        std::stringstream ss;
        ss << "Null target object passed to Deserialize for struct '" << (GetName() ? GetName() : "") << "'";
        throw std::runtime_error(ss.str());
    }

    // Validate JSON shape
    if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
    {
        throw std::runtime_error(std::string("Malformed JSON while deserializing struct '") + (GetName() ? GetName() : "") + "'");
    }

    const auto& arr = value["data"].GetArray();
    const TypeDescriptor_Struct_Impl* impl = SImplConst(this);

    size_t expected = impl ? impl->members.size() : 0;

    // Size mismatch -> throw warning (don't throw runtime error).
    if (static_cast<size_t>(arr.Size()) != expected)
    {
        std::stringstream ss;
        ss << "Array size mismatch while deserializing struct '" << (GetName() ? GetName() : "")
            << "' : expected " << expected << " got " << arr.Size();
        ENGINE_LOG_WARN(ss.str());
        //throw std::runtime_error(ss.str());
    }

    // Nothing to do if no members
    if (!impl || impl->members.empty()) return;

    // Per-member guarded deserialization
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
    {
        const TypeDescriptor_Struct::Member& member = impl->members[i];

        // Check member TypeDescriptor exists
        if (!member.type)
        {
            std::stringstream ss;
            ss << "Missing TypeDescriptor for member index " << i << " of struct '" << (GetName() ? GetName() : "") << "'";
            throw std::runtime_error(ss.str());
        }

        // Resolve member pointer into the target object
        void* member_addr = nullptr;
        try
        {
            member_addr = member.get_ptr(obj);
        }
        catch (const std::exception& e)
        {
            std::stringstream ss;
            ss << "Exception while obtaining pointer for member '" << member.name << "' (index " << i
                << ") of struct '" << (GetName() ? GetName() : "") << "': " << e.what();
            throw std::runtime_error(ss.str());
        }
        if (!member_addr)
        {
            std::stringstream ss;
            ss << "Member pointer is null for member '" << member.name << "' (index " << i
                << ") while deserializing struct '" << (GetName() ? GetName() : "") << "'";
            throw std::runtime_error(ss.str());
        }

        // Deserialize the member and add context on error
        try
        {
            member.type->Deserialize(member_addr, arr[i]);
        }
        catch (const std::exception& e)
        {
            std::stringstream ss;
            ss << "Error deserializing member '" << member.name << "' (index " << i
                << ") of struct '" << (GetName() ? GetName() : "") << "': " << e.what();
            throw std::runtime_error(ss.str());
        }
    }
}

// -------------------------------
// Primitive descriptors (macro-driven)
#define TYPE_DESCRIPTOR(NAME, TYPE) \
  struct TypeDescriptor_##NAME : TypeDescriptor \
  { \
    TypeDescriptor_##NAME() : TypeDescriptor{ #TYPE, sizeof(TYPE) } {} \
    virtual void Dump(const void* obj, std::ostream& os, int) const override \
    { \
      os << #TYPE << "{" << *(const TYPE*)obj << "}"; \
    } \
    virtual void Serialize(const void* obj, std::ostream& os) const override \
    { \
      os << R"({"type":")" << #TYPE << R"(","data":)" << *(const TYPE*)obj << "}"; \
    } \
    virtual void Deserialize(void* obj, const json& value) const override \
    { \
      if (!value.IsObject() || !value.HasMember("data")) throw std::runtime_error("Malformed JSON for primitive type"); \
      TYPE data = ReadJsonAs<TYPE>(value["data"]); \
      *(TYPE*)obj = data; \
    } \
  }; \
  template <> \
  TypeDescriptor* GetPrimitiveDescriptor<TYPE>() \
  { \
    static TypeDescriptor_##NAME type_desc; \
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex()); \
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc; \
    return &type_desc; \
  }

TYPE_DESCRIPTOR(Int, int)
TYPE_DESCRIPTOR(Unsigned, unsigned)
TYPE_DESCRIPTOR(LongLong, int64_t)
TYPE_DESCRIPTOR(UnsignedLongLong, uint64_t)
TYPE_DESCRIPTOR(Double, double)
TYPE_DESCRIPTOR(Float, float)

// bool specialization
struct TypeDescriptor_Bool : TypeDescriptor
{
    TypeDescriptor_Bool() : TypeDescriptor{ "bool", sizeof(bool) } {}
    virtual void Dump(const void* obj, std::ostream& os, int) const override { os << "bool{" << (*(const bool*)obj ? "true" : "false") << "}"; }
    virtual void Serialize(const void* obj, std::ostream& os) const override { os << R"({"type":")" << "bool" << R"(","data":)" << ((*(const bool*)obj) ? "true" : "false") << "}"; }
    virtual void Deserialize(void* obj, const json& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data")) throw std::runtime_error("Malformed JSON for bool");
        bool data = ReadJsonAs<bool>(value["data"]);
        *(bool*)obj = data;
    }
};
template <>
TypeDescriptor* GetPrimitiveDescriptor<bool>()
{
    static TypeDescriptor_Bool type_desc;
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
    return &type_desc;
}

// std::string specialization
struct TypeDescriptor_StdString : TypeDescriptor
{
    TypeDescriptor_StdString() : TypeDescriptor{ "std::string", sizeof(std::string) } {}

    virtual void Dump(const void* obj, std::ostream& os, int) const override
    {
        os << "std::string{" << *(const std::string*)obj << "}";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const std::string& data = *(const std::string*)obj;
        std::ostringstream escaped;
        for (unsigned char c : data)
        {
            switch (c)
            {
            case '\\': escaped << "\\\\"; break;
            case '"': escaped << "\\\""; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 0x20 || c > 0x7E)
                {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' ');
                }
                else escaped << c;
                break;
            }
        }
        os << R"({"type":")" << "std::string" << R"(","data":")" << escaped.str() << R"("})";
    }

    virtual void Deserialize(void* obj, const json& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsString()) throw std::runtime_error("Malformed JSON for std::string");
        std::string s = ReadJsonAs<std::string>(value["data"]);
        *(std::string*)obj = s;
    }
};

template <>
TypeDescriptor* GetPrimitiveDescriptor<std::string>()
{
    static TypeDescriptor_StdString type_desc;
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
    return &type_desc;
}

// -------------------------------
// Template descriptor implementations (StdVector, StdUnorderedMap, StdSharedPtr, StdPair)

// TypeDescriptor_StdVector::Dump / Serialize / Deserialize
void TypeDescriptor_StdVector::Dump(const void* obj, std::ostream& os, int indent_level) const
{
    const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
    size_t num_items = get_size(obj);
    os << "\n" << ToString();
    if (num_items == 0) { os << "{}\n"; return; }
    os << "\n" << std::string(4 * indent_level, ' ') << "{\n";
    for (size_t index = 0; index < num_items; ++index)
    {
        os << std::string(4 * (indent_level + 1), ' ') << "[" << index << "]\n" << std::string(4 * (indent_level + 1), ' ');
        item_type->Dump(get_item(obj, index), os, indent_level + 1);
        os << "\n";
    }
    os << std::string(4 * indent_level, ' ') << "}\n";
}

void TypeDescriptor_StdVector::Serialize(const void* obj, std::ostream& os) const
{
    const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
    size_t num_items = get_size(obj);
    if (num_items == 0)
    {
        os << R"({"type":")" << ToString() << R"(","data":[]})";
        return;
    }
    os << R"({"type":")" << ToString() << R"(","data":[)";
    for (size_t index = 0; index < num_items; ++index)
    {
        item_type->Serialize(get_item(obj, index), os);
        if (index < num_items - 1) os << ",";
    }
    os << "]}";
}

void TypeDescriptor_StdVector::Deserialize(void* obj, const rapidjson::Value& value) const
{
    const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
    if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
        throw std::runtime_error("Malformed JSON while deserializing std::vector");
    const auto& arr = value["data"].GetArray();
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
    {
        item_type->Deserialize(set_item(obj, i), arr[i]);
    }
}

// TypeDescriptor_StdUnorderedMap::Dump / Serialize / Deserialize
template <typename K, typename V>
void DumpUnorderedMapImpl(const void* obj, std::ostream& os, int indent_level, TypeDescriptor* key_type, TypeDescriptor* value_type)
{
    const auto& map = *(const std::unordered_map<K, V>*)obj;
    os << "\n" << std::string(4 * indent_level, ' ') << "std::unordered_map<" << key_type->ToString() << ", " << value_type->ToString() << ">\n" << std::string(4 * indent_level, ' ') << "{\n";
    for (const auto& pair : map)
    {
        os << std::string(4 * (indent_level + 1), ' ');
        key_type->Dump(&pair.first, os, indent_level + 1);
        os << ": ";
        value_type->Dump(&pair.second, os, indent_level + 1);
        os << "\n";
    }
    os << std::string(4 * indent_level, ' ') << "}\n";
}

template <typename K, typename V>
void SerializeUnorderedMapImpl(const void* obj, std::ostream& os, TypeDescriptor* key_type, TypeDescriptor* value_type)
{
    const auto& map = *(const std::unordered_map<K, V>*)obj;
    os << R"({"type":")" << std::string("std::unordered_map<") + key_type->ToString() + ", " + value_type->ToString() + R"(","data":[)";
    bool first = true;
    for (const auto& pair : map)
    {
        if (!first) os << ",";
        first = false;
        os << "[";
        key_type->Serialize(&pair.first, os);
        os << ",";
        value_type->Serialize(&pair.second, os);
        os << "]";
    }
    os << "]}";
}

template <typename K, typename V>
void DeserializeUnorderedMapImpl(void* obj, const rapidjson::Value& value, TypeDescriptor* key_type, TypeDescriptor* value_type)
{
    if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
        throw std::runtime_error("Malformed JSON while deserializing std::unordered_map");
    auto& map = *(std::unordered_map<K, V>*)obj;
    const auto& arr = value["data"].GetArray();
    for (rapidjson::SizeType i = 0; i < arr.Size(); i++)
    {
        K key; V val;
        key_type->Deserialize(&key, arr[i][0]);
        value_type->Deserialize(&val, arr[i][1]);
        map[key] = val;
    }
}

template <typename KeyType, typename ValueType>
void TypeDescriptor_StdUnorderedMap<KeyType, ValueType>::Dump(const void* obj, std::ostream& os, int indent_level) const
{
    DumpUnorderedMapImpl<KeyType, ValueType>(obj, os, indent_level, key_type, value_type);
}

template <typename KeyType, typename ValueType>
void TypeDescriptor_StdUnorderedMap<KeyType, ValueType>::Serialize(const void* obj, std::ostream& os) const
{
    SerializeUnorderedMapImpl<KeyType, ValueType>(obj, os, key_type, value_type);
}

template <typename KeyType, typename ValueType>
void TypeDescriptor_StdUnorderedMap<KeyType, ValueType>::Deserialize(void* obj, const rapidjson::Value& value) const
{
    DeserializeUnorderedMapImpl<KeyType, ValueType>(obj, value, key_type, value_type);
}

// TypeDescriptor_StdSharedPtr<T> Dump/Serialize/Deserialize
template <typename T>
void TypeDescriptor_StdSharedPtr<T>::Dump(const void* obj, std::ostream& os, int indent_level) const
{
    const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<T>*>(obj);
    if (shared_ptr)
    {
        os << "\n" << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n" << std::string(4 * (indent_level + 1), ' ');
        item_type->Dump(shared_ptr.get(), os, indent_level + 1);
        os << "\n" << std::string(4 * indent_level, ' ') << "}\n";
    }
    else { os << "null"; }
}

template <typename T>
void TypeDescriptor_StdSharedPtr<T>::Serialize(const void* obj, std::ostream& os) const
{
    const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<T>*>(obj);
    if (shared_ptr) item_type->Serialize(shared_ptr.get(), os);
    else os << "null";
}

template <typename T>
void TypeDescriptor_StdSharedPtr<T>::Deserialize(void* obj, const rapidjson::Value& value) const
{
    if (value.IsNull()) { *reinterpret_cast<std::shared_ptr<T>*>(obj) = nullptr; }
    else
    {
        std::shared_ptr<T> sp = std::make_shared<T>();
        item_type->Deserialize(sp.get(), value);
        *reinterpret_cast<std::shared_ptr<T>*>(obj) = sp;
    }
}

// TypeDescriptor_StdSharedPtr<void> specialization behavior (binary blob w/ size prefix)
void TypeDescriptor_StdSharedPtr<void>::Dump(const void* obj, std::ostream& os, int) const
{
    const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<void>*>(obj);
    if (shared_ptr)
    {
        os << "\n" << ToString() << "\n" << "{\n" << shared_ptr.get() << "\n" << "}\n";
    }
    else { os << "null"; }
}

void TypeDescriptor_StdSharedPtr<void>::Serialize(const void* obj, std::ostream& os) const
{
    const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<void>*>(obj);
    if (!shared_ptr) { os << "null"; return; }

    void* ptr = shared_ptr.get();
    // The contract: first sizeof(uint64_t) bytes hold the payload size (little-endian)
    uint64_t data_size = 0;
    std::memcpy(&data_size, ptr, sizeof(uint64_t));
    //uint64_t le_size = ToLittleEndian_u64(data_size);

    uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
    std::vector<uint8_t> data(byte_ptr, byte_ptr + sizeof(uint64_t) + static_cast<size_t>(data_size));
    std::string serialized_data = Base64_Encode(data);

    os << R"({"type":")" << ToString() << R"(","data":")" << serialized_data << R"("})";
}

void TypeDescriptor_StdSharedPtr<void>::Deserialize(void* obj, const rapidjson::Value& value) const
{
    if (value.IsNull()) { *reinterpret_cast<std::shared_ptr<void>*>(obj) = nullptr; return; }
    if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsString())
    {
        throw std::runtime_error("Malformed JSON for std::shared_ptr<void>");
    }

    std::string data = value["data"].GetString();
    std::vector<uint8_t> decoded = Base64_Decode(data);
    if (decoded.size() < sizeof(uint64_t)) throw std::runtime_error("Decoded shared_ptr<void> too small");

    uint64_t stored_size = 0;
    std::memcpy(&stored_size, decoded.data(), sizeof(uint64_t));
    uint64_t data_size = FromLittleEndian_u64(stored_size);

    if (decoded.size() != sizeof(uint64_t) + data_size) throw std::runtime_error("Decoded shared_ptr<void> length mismatch");

    void* ptr = new uint8_t[sizeof(uint64_t) + static_cast<size_t>(data_size)];
    std::memcpy(ptr, decoded.data(), decoded.size());

    std::shared_ptr<void> sp(ptr, [](void* p) { delete[] reinterpret_cast<uint8_t*>(p); });
    *reinterpret_cast<std::shared_ptr<void>*>(obj) = sp;
}

// TypeDescriptor_StdPair implementations
template <typename A, typename B>
void TypeDescriptor_StdPair<A, B>::Dump(const void* obj, std::ostream& os, int indent_level) const
{
    const auto& pair = *(const std::pair<A, B>*)obj;
    os << "\n" << std::string(4 * indent_level, ' ') << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n" << std::string(4 * (indent_level + 1), ' ');
    first_type->Dump(&pair.first, os, indent_level + 1);
    os << ",\n" << std::string(4 * (indent_level + 1), ' ');
    second_type->Dump(&pair.second, os, indent_level + 1);
    os << "\n" << std::string(4 * indent_level, ' ') << "}\n";
}

template <typename A, typename B>
void TypeDescriptor_StdPair<A, B>::Serialize(const void* obj, std::ostream& os) const
{
    const auto& pair = *(const std::pair<A, B>*)obj;
    os << R"({"type":")" << ToString() << R"(","data":[)";
    first_type->Serialize(&pair.first, os);
    os << ",";
    second_type->Serialize(&pair.second, os);
    os << "]}";
}

template <typename A, typename B>
void TypeDescriptor_StdPair<A, B>::Deserialize(void* obj, const rapidjson::Value& value) const
{
    if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
    {
        throw std::runtime_error("Malformed JSON while deserializing std::pair");
    }
    const auto& arr = value["data"].GetArray();
    if (arr.Size() != 2) throw std::runtime_error("std::pair expects array of size 2");
    first_type->Deserialize(&((std::pair<A, B>*)obj)->first, arr[0]);
    second_type->Deserialize(&((std::pair<A, B>*)obj)->second, arr[1]);
}

// -------------------------------
// GUID_128 descriptor
TypeDescriptor_GUID128::TypeDescriptor_GUID128()
    : TypeDescriptor("GUID_128", sizeof(GUID_128))
{
}

void TypeDescriptor_GUID128::Dump(const void* obj, std::ostream& os, int) const
{
    os << GUIDUtilities::ConvertGUID128ToString(*reinterpret_cast<const GUID_128*>(obj));
}

void TypeDescriptor_GUID128::Serialize(const void* obj, std::ostream& os) const
{
    os << "\"" << GUIDUtilities::ConvertGUID128ToString(*reinterpret_cast<const GUID_128*>(obj)) << "\"";
}

void TypeDescriptor_GUID128::Deserialize(void* obj, const rapidjson::Value& value) const
{
    if (!value.IsString()) throw std::runtime_error("GUID_128 must be a string");
    *reinterpret_cast<GUID_128*>(obj) = GUIDUtilities::ConvertStringToGUID128(value.GetString());
}

// TypeResolver<GUID_128>::Get
TypeDescriptor* TypeResolver<GUID_128>::Get()
{
    static TypeDescriptor_GUID128 type_desc;
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
    return &type_desc;
}

#ifdef ANDROID
// primitives.cpp
template<>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<long long>() {
    static TypeDescriptor_LongLong type_desc;
    // For lines 726 and 734, replace with:
    const char* desc_name = typeid(type_desc).name();  // Or a hardcoded string like "long long"
    if (TYPE_DESCRIPTOR_LOOKUP.count(desc_name) == 0) TYPE_DESCRIPTOR_LOOKUP[desc_name] = &type_desc;
    return &type_desc;
}

template<>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<unsigned long long>() {
    static TypeDescriptor_UnsignedLongLong type_desc;
    const char* desc_name = typeid(type_desc).name();  // Or a hardcoded string like "unsigned long long"
    if (TYPE_DESCRIPTOR_LOOKUP.count(desc_name) == 0) TYPE_DESCRIPTOR_LOOKUP[desc_name] = &type_desc;
    return &type_desc;
}
#endif

#pragma endregion
