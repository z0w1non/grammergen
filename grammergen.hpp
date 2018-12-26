#ifndef GRAMMERGEN_HPP
#define GRAMMERGEN_HPP

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <random>
#include <iostream>
#include <fstream>
#include <functional>
#include <cctype>

namespace grammergen {

class grammer_generator;

class grammer {
    friend class grammer_generator;

public:
    grammer() {}

    grammer(
        const std::shared_ptr<grammer> & first,
        const std::shared_ptr<grammer> & second
    )
        : first{first}
        , second{second}
    {}

    virtual ~grammer() {}

    virtual auto parse(std::string_view) const -> std::vector<std::string_view> = 0;

    virtual auto size() const -> std::size_t {
        std::size_t size = 1;
        if (first)
            size += first->size();
        if (second)
            size += second->size();
        return size;
    }

    virtual auto match(std::string_view str) const -> bool {
        auto candidates = parse(str);
        if (candidates.empty())
            return false;
        for (const auto & candidate : candidates)
            if (!candidate.empty())
                return false;
        return true;
    }

    virtual auto evaluate(std::string_view str) const -> double {
        return match(str) ? (1.0 / size()) : 0.0;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> = 0;

    virtual auto print(std::ostream & out) const -> void {
        out << "(" << name();
        if (first || second)
            out << " ";
        if (first)
            first->print(out);
        if (first && second)
            out << " ";
        if (second)
            second->print(out);
        out << ")";
    }

    virtual auto name() const -> const char * = 0;

protected:
    std::shared_ptr<grammer> first;
    std::shared_ptr<grammer> second;
    std::shared_ptr<void> impl_ptr;
};

auto operator <<(std::ostream & out, const grammer & grm) -> std::ostream & {
    grm.print(out);
    return out;
}

class cat : public grammer {
public:
    using grammer::grammer;

    virtual ~cat() {}

    virtual auto parse(std::string_view str) const -> std::vector<std::string_view> override {
        std::vector<std::string_view> candidates;
        for (auto rest : first->parse(str))
            for (auto s : second->parse(rest))
                candidates.push_back(s);
        return candidates;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        return std::make_shared<cat>(first->clone(), second->clone());
    }

    virtual auto name() const -> const char * override {
        return "cat";
    }
};

class word : public grammer {
private:
    class impl_type {
    public:
        impl_type(std::string_view view) : str{view} {}
        std::string str;
    };

public:
    word(std::string_view view) {
        impl_ptr = std::make_shared<impl_type>(view);
    }

    virtual ~word() {}

    virtual auto parse(std::string_view str) const -> std::vector<std::string_view> override {
        std::vector<std::string_view> candidates;
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        if (impl.str == std::string_view(str.data(), impl.str.size()))
            candidates.emplace_back(str.data() + impl.str.size(), str.size() - impl.str.size());
        return candidates;
    }

    virtual auto size() const -> std::size_t {
        return 16;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        std::make_shared<word>(impl.str);
    }

    virtual auto print(std::ostream & out) const -> void override {
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        out << "(" << name() << " " << impl.str << ")";
    }

    virtual auto name() const -> const char * override {
        return "word";
    }
};

class or_ : public grammer {
public:
    using grammer::grammer;

    virtual auto parse(std::string_view str) const -> std::vector<std::string_view> override {
        std::vector<std::string_view> candidates;
        if (first)
            for (auto rest : first->parse(str))
                candidates.push_back(rest);
        if (second)
            for (auto rest : second->parse(str))
                candidates.push_back(rest);
        return candidates;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        return std::make_shared<or_>(first->clone(), second->clone());
    }

    virtual auto name() const -> const char * override {
        return "or";
    }
};

class grammer_generator {
public:
    static auto generate_node() -> std::shared_ptr<grammer> {
        static bool has_been_initialized = false;
        static std::vector<std::function<std::shared_ptr<grammer>()>> functions;
        if (!has_been_initialized) {
            functions.push_back([](){return std::make_shared<cat>();});
            functions.push_back([](){return std::make_shared<or_>();});
            functions.push_back([](){
                char c = static_cast<char>(random_integral<>(0, 0xff));
                return std::make_shared<word>(std::string_view{&c, 1});});
            has_been_initialized = true;
        }
        std::size_t index = random_integral<std::size_t>(0, functions.size() - 1);
        return functions[index]();
    }

    static auto generate_tree(std::size_t node_number) -> std::shared_ptr<grammer> {
        std::vector<std::reference_wrapper<std::shared_ptr<grammer>>> terminals;
        if (node_number == 0)
            throw std::logic_error("node_number must be greater than zero.");
        auto root = generate_node();
        terminals.push_back(root->first);
        terminals.push_back(root->second);
        for (std::size_t i = 1; i < node_number; ++i) {
            auto temp = generate_node();
            std::size_t index = random_integral<std::size_t>(0, terminals.size() - 1);
            terminals[index].get() = temp;
            terminals.erase(terminals.begin() + index);
            terminals.push_back(temp->first);
            terminals.push_back(temp->second);
        }
        return root;
    }

    static auto random_select_node(const std::shared_ptr<grammer> & grm) -> std::shared_ptr<grammer> {
        struct impl {
            static auto push(
                const std::shared_ptr<grammer> & grm,
                std::vector<std::shared_ptr<grammer>> & vct
            ) -> void
            {
                vct.push_back(grm);
                if (grm->first)
                    push(grm->first, vct);
                if (grm->second)
                    push(grm->second, vct);
            }
        };
        std::vector<std::shared_ptr<grammer>> vct;
        impl::push(grm, vct);
        return vct[random_integral<std::size_t>(0, vct.size() - 1)];
    }

    template<typename Integral = int>
    static auto random_integral(Integral min, Integral max) -> Integral {
        static std::mt19937 mt{std::random_device{}()};
        return std::uniform_int_distribution<Integral>{min, max}(mt);
    }
};

class generic_programming {
public:
    generic_programming() {}

    auto init_grammer(std::size_t tree_number, std::size_t node_number) -> void {
        _grammer_list.resize(tree_number);
        for (auto & ptr : _grammer_list)
            ptr = grammer_generator::generate_tree(node_number);
    }

    auto run()
        -> void
    {
        using pair = std::pair<std::shared_ptr<grammer>, double>;
        std::vector<pair> pairs;
        for (const auto & grm : _grammer_list)
            pairs.emplace_back(grm, evaluate(*grm));
        std::sort(std::begin(pairs), std::end(pairs), [](auto && a, auto && b){
            return a.second > b.second;
        });
    }

    auto read_input(std::string_view path) -> void {
        std::ifstream in{std::string(path)};
        std::string line;
        while (std::getline(in, line))
            append_input(line);
    }

    auto read_grammer(std::string_view path) -> void {
        // TODO
    }

    auto write_grammer(std::string_view path) const -> void {
        // TODO
    }

    auto print_grammer() const -> void {
        for (const auto & grm : _grammer_list)
            std::cout << *grm << std::endl;
    }

    auto append_input(std::string_view str)
        -> void
    {
        _input_list.emplace_back(str);
    }

private:
    auto evaluate(const grammer & grm) const
        -> double
    {
        double value = 0;
        for (const auto & input : _input_list)
            value += grm.evaluate(input);
        return value;
    }

    std::vector<std::string> _input_list;
    std::vector<std::shared_ptr<grammer>> _grammer_list;
    std::set<std::string> _dictionary;
};

} // namespace grammergen

#endif
