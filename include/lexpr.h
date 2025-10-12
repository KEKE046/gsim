#pragma once
#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

namespace sexpr {

struct Emitter {
    int indent = 0;
    int inline_depth = 0;
    bool nosep = false;
    bool need_inline = false;
    bool just_begin = false;
    std::ostream & out;
    inline Emitter(std::ostream & out): out(out) {}
    inline void sep() {
        if(inline_depth == 0) {
            if(just_begin && nosep) {
                nosep = false;
                // do nothing
            } else {
                nosep = false;
                out << "\n";
                for(int i = 0; i < indent; i++) {
                    out << "  ";
                }
            }
        }
        else {
            if(nosep) {
                nosep = false;
            } else {
                out << " ";
            }
        }
        just_begin = false;
    }
    inline void keyword(std::string_view name) {
        sep();
        out << name;
    }
    inline void named(std::string_view name) {
        sep();
        out << '(' << name;
        indent++;
        if(need_inline) {
            need_inline = false;
            inline_depth++;
        }
        just_begin = true;
    }
    inline void tup() {
        sep();
        out << "#(";
        indent++;
        if(need_inline) {
            need_inline = false;
            inline_depth++;
        }
        just_begin = true;
        nosep = true;
    }
    inline void list() {
        nosep = false;
        sep();
        out << "(";
        indent++;
        if(need_inline) {
            need_inline = false;
            inline_depth++;
        }
        just_begin = true;
        if(inline_depth) {
            nosep = true;
        }
    }
    inline void end() {
        indent--;
        nosep = true;
        sep();
        out << ')';
    }
    inline void string(std::string_view data) {
        sep();
        out << '"';
        for(char c: data) {
            switch(c) {
                case '\\': out << "\\\\"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                case '\"': out << "\\\""; break;
                // case '\'': out << "\\\'"; break;
                default: out << c; break;
            }
        }
        out << '"';
    }
    template<typename T, typename = std::enable_if<std::is_integral_v<T>>>
    inline void integer(const T & t) {
        sep();
        out << t;
    }
    inline void boolean(bool v) {
        sep();
        if(v) out << "#t";
        else out << "#f";
    }
    inline Emitter & operator << (std::string_view s) {
        string(s);
        return *this;
    }
    inline Emitter & operator << (std::function<void(Emitter&)> controller) {
        controller(*this);
        return *this;
    }
    template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
    inline Emitter & operator << (T t) {
        if constexpr(std::is_same_v<T, bool>) {
            boolean(t);
        } else {
            integer(t);
        }
        return *this;
    }
};

inline auto kw(std::string_view s) {
    return [=](Emitter & e){ e.keyword(s); };
}
inline void end(Emitter & e) { e.end(); }
inline void list(Emitter & e) { e.list(); }
inline auto named(std::string_view s) {
    return [=](Emitter & e) { e.named(s); };
}
inline auto pretty(Emitter & e) {
    if(e.need_inline) {
        e.need_inline = false;
    } else {
        e.inline_depth--;
    }
}
inline auto inlined(Emitter & e) {
    e.need_inline = true;
}
template<typename T>
inline auto kv(std::string_view s, const T & t) {
    return [=](Emitter & e) { e << inlined << named(s) << kw(".") << t << end << pretty; };
}
template<typename T>
inline auto kvs(std::string_view s, const T & t) {
    return [=](Emitter & e) {
        e << inlined << named(s);
        for(auto & item: t) e << item;
        e << end << pretty;
    };
}
inline void tup(Emitter & e) { e.tup(); }
} // namespace sexpr
