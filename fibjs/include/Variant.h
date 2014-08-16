/*
 * Variant.h
 *
 *  Created on: Jul 26, 2012
 *      Author: lion
 */

#include <v8/v8.h>
#include <string>
#include "date.h"
#include "obj_ptr.h"

#ifndef VARIANT_H_
#define VARIANT_H_

namespace fibjs
{

inline bool IsEmpty(v8::Local<v8::Value> &v)
{
    return v.IsEmpty() || v->IsUndefined() || v->IsNull();
}

inline void extend(const v8::Local<v8::Object> src,
                   v8::Local<v8::Object> &dest, bool bDataOnly = true)
{
    v8::TryCatch try_catch;
    v8::Local<v8::Array> ks = src->GetPropertyNames();
    int len = ks->Length();
    int i;

    for (i = 0; i < len; i++)
    {
        v8::Local<v8::Value> k = ks->Get(i);
        v8::Local<v8::Value> v = src->Get(k);

        if (!bDataOnly || (!v.IsEmpty() && !v->IsFunction()))
            dest->Set(k, v);
    }
}

class object_base;

class Variant
{
public:
    enum Type
    {
        VT_Undefined = 0,
        VT_Null,
        VT_Boolean,
        VT_Integer,
        VT_Long,
        VT_Number,
        VT_Date,
        VT_String,
        VT_Object,
        VT_JSValue,
        VT_Type = 255,
        VT_Persistent = 256
    };

public:
    Variant() :
        m_type(VT_Undefined)
    {
    }

    Variant(const Variant &v) :
        m_type(VT_Undefined)
    {
        operator=(v);
    }

    Variant(const std::string &v) :
        m_type(VT_Undefined)
    {
        operator=(v);
    }

    Variant(const char *v) :
        m_type(VT_Undefined)
    {
        operator=(v);
    }

    Variant(v8::Local<v8::Value> v) :
        m_type(VT_Undefined)
    {
        operator=(v);
    }

    ~Variant()
    {
        clear();
    }

    void clear()
    {
        if (type() == VT_String)
            ((std::string *) m_Val.strVal)->~basic_string();
        else if (type() == VT_Object && m_Val.objVal)
            m_Val.objVal->Unref();
        else if (type() == VT_JSValue)
        {
            if (isPersistent())
            {
                v8::Persistent<v8::Value> &jsobj =
                    *(v8::Persistent<v8::Value> *) m_Val.jsVal;
                jsobj.Reset();
                jsobj.~Persistent();
            }
            else
            {
                v8::Local<v8::Value> &jsobj =
                    *(v8::Local<v8::Value> *) m_Val.jsVal;
                jsobj.~Local();
            }
        }

        set_type(VT_Undefined);
    }

    Variant &operator=(const Variant &v)
    {
        if (v.type() == VT_String)
            return operator=(*(std::string *) v.m_Val.strVal);

        if (v.type() == VT_Object)
            return operator=(v.m_Val.objVal);

        if (v.type() == VT_JSValue)
        {
            if (v.isPersistent())
                return operator=(
                           v8::Local<v8::Value>::New(isolate,
                                                     *(v8::Persistent<v8::Value> *) v.m_Val.jsVal));
            else
                return operator=(*(v8::Local<v8::Value> *) v.m_Val.jsVal);
        }

        clear();
        set_type(v.type());
        m_Val.longVal = v.m_Val.longVal;

        return *this;
    }

    Variant &operator=(int32_t v)
    {
        clear();

        set_type(VT_Integer);
        m_Val.intVal = v;

        return *this;
    }

    Variant &operator=(int64_t v)
    {
        clear();

        if (v >= -2147483648ll && v <= 2147483647ll)
        {
            set_type(VT_Integer);
            m_Val.intVal = (int32_t) v;
        }
        else
        {
            set_type(VT_Long);
            m_Val.longVal = v;
        }

        return *this;
    }

    Variant &operator=(double v)
    {
        clear();

        set_type(VT_Number);
        m_Val.dblVal = v;

        return *this;
    }

    Variant &operator=(date_t &v)
    {
        clear();

        set_type(VT_Date);
        *(date_t *) m_Val.dateVal = v;

        return *this;
    }

    Variant &operator=(const std::string &v)
    {
        if (type() != VT_String)
        {
            clear();
            set_type(VT_String);
            new (((std::string *) m_Val.strVal)) std::string(v);
        }
        else
            *(std::string *) m_Val.strVal = v;

        return *this;
    }

    Variant &operator=(const char *v)
    {
        std::string s(v);

        return operator=(s);
    }

    Variant &operator=(object_base *v)
    {
        return operator=((obj_base *) v);
    }

    Variant &operator=(v8::Local<v8::Value> v);

    Variant &operator=(v8::Local<v8::Function> v)
    {
        return operator=(v8::Local<v8::Value>::Cast(v));
    }

    Variant &operator=(v8::Local<v8::Object> v)
    {
        return operator=(v8::Local<v8::Value>::Cast(v));
    }

    Variant &setNull()
    {
        clear();

        set_type(VT_Null);
        return *this;
    }

    Type type() const
    {
        return (Type) (m_type & VT_Type);
    }

    void set_type(Type t)
    {
        m_type = (Type) (t | (m_type & VT_Persistent));
    }

    bool isUndefined()
    {
        return type() == VT_Undefined;
    }

    void toPersistent()
    {
        m_type = (Type) (m_type | VT_Persistent);
    }

    bool isPersistent() const
    {
        return (m_type & VT_Persistent) == VT_Persistent;
    }

    size_t size() const
    {
        if (type() != VT_String)
            return sizeof(Variant);

        return sizeof(Variant) + ((std::string *) m_Val.strVal)->length();
    }

    std::string string() const
    {
        if (type() != VT_String)
            return "";
        return *(std::string *) m_Val.strVal;
    }

    operator v8::Local<v8::Value>() const;

    operator int32_t() const
    {
        if (type() != VT_Integer)
            return 0;
        return m_Val.intVal;
    }

    void parseNumber(const char *str, int len = -1);
    void parseDate(const char *str, int len = -1)
    {
        set_type(VT_Date);
        ((date_t *) m_Val.dateVal)->parse(str, len);
    }

    bool toString(std::string &retVal);

    obj_base *object() const
    {
        if (type() != VT_Object)
            return NULL;
        return m_Val.objVal;
    }

private:
    Variant &operator=(obj_base *v)
    {
        clear();

        set_type(VT_Object);
        m_Val.objVal = v;
        if (m_Val.objVal)
            m_Val.objVal->Ref();

        return *this;
    }

private:
    Type m_type;
    union
    {
        bool boolVal;
        int32_t intVal;
        int64_t longVal;
        double dblVal;
        obj_base *objVal;
        char dateVal[sizeof(date_t)];
        char strVal[sizeof(std::string)];
        char jsVal[sizeof(v8::Persistent<v8::Value>)];
    } m_Val;
};

class VariantEx: public Variant
{

public:
    VariantEx()
    {
        toPersistent();
    }

    VariantEx(const Variant &v)
    {
        toPersistent();
        operator=(v);
    }

    VariantEx(const VariantEx &v)
    {
        toPersistent();
        operator=(v);
    }

    VariantEx(v8::Local<v8::Value> v)
    {
        toPersistent();
        operator=(v);
    }

    VariantEx(const std::string &v)
    {
        toPersistent();
        operator=(v);
    }

    VariantEx(const char *v)
    {
        toPersistent();
        operator=(v);
    }

    Variant &operator=(const VariantEx &v)
    {
        Variant::operator=((const Variant)v);
        return *this;
    }

    template<typename T>
    VariantEx &operator=(T v)
    {
        Variant::operator=(v);
        return *this;
    }

    operator v8::Local<v8::Value>() const
    {
        return Variant::operator v8::Local<v8::Value>();
    }
};

} /* namespace fibjs */
#endif /* VARIANT_H_ */
