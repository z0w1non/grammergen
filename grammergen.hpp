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
#include <type_traits>
#include <deque>

namespace grammergen {

class context {
public:
    std::size_t match_count{};
    std::size_t compare_count{};
};

template<typename T>
using evaluated = std::pair<T, double>;

class grammer : public std::enable_shared_from_this<grammer> {
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

    virtual auto parse(std::string_view, context & ctx) const -> std::vector<std::string_view> = 0;

    virtual auto size() const -> std::size_t {
        std::size_t size = 1;
        if (first)
            size += first->size();
        if (second)
            size += second->size();
        return size;
    }

    static auto match(std::vector<std::string_view> & candidates) -> bool {
        if (candidates.empty())
            return false;
        for (const auto & candidate : candidates)
            if (!candidate.empty())
                return false;
        return true;
    }

    virtual auto evaluate(std::string_view str) const -> double {
        context ctx;
        auto candicates = parse(str, ctx);
        if (match(candicates))
            return 1.0;
        return static_cast<double>(ctx.match_count)/* / ctx.compare_count */;
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

    virtual auto operand_number() const -> std::size_t = 0;

    std::shared_ptr<grammer> first;
    std::shared_ptr<grammer> second;
    std::shared_ptr<void> impl_ptr;
};

auto operator <<(std::ostream & out, const grammer & grm) -> std::ostream & {
    grm.print(out);
    return out;
}

class join : public grammer {
public:
    using grammer::grammer;

    virtual ~join() {}

    virtual auto parse(std::string_view str, context & ctx) const -> std::vector<std::string_view> override {
        ctx.compare_count += size();
        std::vector<std::string_view> candidates;
        if (first && second)
            for (auto rest : first->parse(str, ctx))
                for (auto s : second->parse(rest, ctx))
                    candidates.push_back(s);
        return candidates;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        std::shared_ptr<grammer> first_clone, second_clone;
        if (first)
            first_clone = first->clone();
        if (second)
            second_clone = second->clone();
        return std::make_shared<join>(first_clone, second_clone);
    }

    virtual auto name() const -> const char * override {
        return "+";
    }

    virtual auto operand_number() const -> std::size_t override {
        return 2;
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

    virtual auto parse(std::string_view str, context & ctx) const -> std::vector<std::string_view> override {
        ctx.compare_count += size();
        std::vector<std::string_view> candidates;
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        if (impl.str == std::string_view(str.data(), impl.str.size()))
            candidates.emplace_back(str.data() + impl.str.size(), str.size() - impl.str.size());
        ctx.match_count += candidates.size();
        return candidates;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        return std::make_shared<word>(impl.str);
    }

    virtual auto print(std::ostream & out) const -> void override {
        const auto & impl = *reinterpret_cast<impl_type*>(impl_ptr.get());
        out << "\"" << impl.str << "\"";
    }

    virtual auto name() const -> const char * override {
        return "word";
    }

    virtual auto operand_number() const -> std::size_t override {
        return 0;
    }
};

class or_ : public grammer {
public:
    using grammer::grammer;

    virtual auto parse(std::string_view str, context & ctx) const -> std::vector<std::string_view> override {
        ctx.compare_count += size();
        std::vector<std::string_view> candidates;
        if (first)
            for (auto rest : first->parse(str, ctx))
                candidates.push_back(rest);
        if (second)
            for (auto rest : second->parse(str, ctx))
                candidates.push_back(rest);
        return candidates;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        std::shared_ptr<grammer> first_clone, second_clone;
        if (first)
            first_clone = first->clone();
        if (second)
            second_clone = second->clone();
        return std::make_shared<or_>(first_clone, second_clone);
    }

    virtual auto name() const -> const char * override {
        return "|";
    }

    virtual auto operand_number() const -> std::size_t override {
        return 2;
    }
};

class optional : public grammer {
public:
    using grammer::grammer;

    virtual auto parse(std::string_view str, context & ctx) const -> std::vector<std::string_view> override {
        ctx.compare_count += size();
        std::vector<std::string_view> candidates;
        if (first)
            for (auto rest : first->parse(str, ctx))
                candidates.push_back(rest);
        candidates.push_back(str);
        return candidates;
    }

    virtual auto size() const -> std::size_t override {
        std::size_t size = 1;
        if (first)
            size += first->size() * 2;
        return size;
    }

    virtual auto clone() const -> std::shared_ptr<grammer> override {
        std::shared_ptr<grammer> first_clone, second_clone;
        if (first)
            first_clone = first->clone();
        return std::make_shared<optional>(first_clone, std::shared_ptr<grammer>{});
    }

    virtual auto name() const -> const char * override {
        return "?";
    }

    virtual auto operand_number() const -> std::size_t override {
        return 1;
    }
};

template<typename Integral = int>
auto random_integral(Integral min, Integral max) -> Integral {
    static std::mt19937 mt{std::random_device{}()};
    return std::uniform_int_distribution<Integral>{min, max}(mt);
}

template<typename RealType = double>
auto random_floating_point(RealType min, RealType max) -> RealType {
    static std::mt19937 mt{std::random_device{}()};
    return std::uniform_real_distribution<RealType>{min, max}(mt);
}

template<typename Container>
auto random_element(Container && c) {
    auto index = random_integral<typename std::decay_t<Container>::size_type>(0, c.size() - 1);
    return *(c.begin() + index);
}

auto generate_node() -> std::shared_ptr<grammer> {
    static bool has_been_initialized = false;
    static std::vector<std::function<std::shared_ptr<grammer>()>> functions;
    if (!has_been_initialized) {
        functions.push_back([](){return std::make_shared<join>();});
        functions.push_back([](){return std::make_shared<or_>();});
        functions.push_back([](){
            char c;
            while (!std::isprint(c = static_cast<char>(random_integral<>(0, 0xff))));
            return std::make_shared<word>(std::string_view{&c, 1});
        });
        functions.push_back([](){return std::make_shared<optional>();});
        has_been_initialized = true;
    }
    return random_element(functions)();
}

auto optimize_tree(const std::shared_ptr<grammer> & root) -> void {
    struct impl {
        static auto optimize_node(const std::shared_ptr<grammer> & node) -> void {
            if (!node)
                return;
            if (node->operand_number() == 0) {
                node->first = std::shared_ptr<grammer>{};
                node->second = std::shared_ptr<grammer>{};
            }
            if (node->operand_number() == 1) {
                optimize_node(node->first);
                node->second = std::shared_ptr<grammer>{};
            }
            if (node->operand_number() == 2) {
                optimize_node(node->first);
                optimize_node(node->second);
            }
        }
    };
    impl::optimize_node(root);
}

auto generate_tree(std::size_t node_number) -> std::shared_ptr<grammer> {
    std::deque<std::reference_wrapper<std::shared_ptr<grammer>>> terminals;
    if (node_number == 0)
        throw std::logic_error("node_number must be greater than zero.");
    auto root = generate_node();
    if (root->operand_number() >= 1)
        terminals.push_back(root->first);
    if (root->operand_number() >= 2)
        terminals.push_back(root->second);
    for (std::size_t i = 1; i < node_number; ++i) {
        if (terminals.empty())
            break;
        auto temp = generate_node();
        std::size_t index = random_integral<std::size_t>(0, terminals.size() - 1);
        terminals[index].get() = temp;
        terminals.erase(terminals.begin() + index);
        if (temp->operand_number() >= 1)
            terminals.push_back(temp->first);
        if (temp->operand_number() >= 2)
            terminals.push_back(temp->second);
    }
    return root;
}

auto mutate_node(std::shared_ptr<grammer> & node) -> void {
    auto first = node->first;
    auto second = node->second;
    node = generate_node();
    node->first = first;
    node->second = second;
}

auto get_nodes(
    std::shared_ptr<grammer> & root
) -> std::vector<std::reference_wrapper<std::shared_ptr<grammer>>> {
    struct impl {
        static auto push(
            std::shared_ptr<grammer> & grm,
            std::vector<std::reference_wrapper<std::shared_ptr<grammer>>> & vct
        ) -> void
        {
            vct.push_back(std::ref(grm));
            if (grm->first)
                push(grm->first, vct);
            if (grm->second)
                push(grm->second, vct);
        }
    };
    std::vector<std::reference_wrapper<std::shared_ptr<grammer>>> vct;
    impl::push(root, vct);
    return vct;
}

auto random_select_node(
    std::shared_ptr<grammer> & root
) -> std::shared_ptr<grammer> {
    return random_element(get_nodes(root));
}

auto create_crossed_tree(
    const std::shared_ptr<grammer> & a_root,
    const std::shared_ptr<grammer> & b_root
) -> std::pair<std::shared_ptr<grammer>, std::shared_ptr<grammer>> {
    auto a_clone = a_root->clone();
    auto b_clone = b_root->clone();
    auto a_nodes = get_nodes(a_clone);
    auto b_nodes = get_nodes(b_clone);
    std::swap(
        random_element(a_nodes).get(),
        random_element(b_nodes).get()
    );
    return std::make_pair(a_clone, b_clone);
}

auto select_individual(std::vector<std::pair<std::shared_ptr<grammer>, double>> & individuals) -> std::shared_ptr<grammer>{
    if (individuals.empty())
        throw std::logic_error("Individuals number must be not empty.");
    double sum = 0;
    for (auto & individual : individuals)
        sum += individual.second;
    if (sum == 0)
        return random_element(individuals).first;
    double rand = random_floating_point<double>(0, sum);
    for (auto & individual : individuals) {
        rand -= individual.second;
        if (rand <= 0)
            return individual.first;
    }
    return individuals[0].first;
}

class generic_programming {
public:
    generic_programming() {}

    auto init_grammer(std::size_t tree_number, std::size_t node_number) -> void {
        _grammer_list.resize(tree_number);
        for (auto & ptr : _grammer_list)
            ptr = generate_tree(node_number);
    }

    auto set_elite_ratio(double elite_ratio) -> void {
        if (elite_ratio < 0)
            throw std::invalid_argument("elite_ratio must be greater or equal than zero.");
        if (elite_ratio > 1)
            throw std::invalid_argument("elite_ratio must be less than one.");
        _elite_ratio = elite_ratio;
    }

    auto set_mutation_ratio(double mutation_ratio) -> void {
        if (mutation_ratio < 0)
            throw std::invalid_argument("mutation_ratio must be greater or equal than zero.");
        if (mutation_ratio > 1)
            throw std::invalid_argument("mutation_ratio must be less than one.");
        _mutation_ratio = mutation_ratio;
    }

    auto set_max_unmodified_count(std::size_t max_unmodified_count) -> void {
        _max_unmodified_count = max_unmodified_count;
    }

    auto run() -> void {
        std::size_t unmodified_count = 0;
        double prev_eval = update();
        while (true) {
            double eval = update();
            if (eval == prev_eval) {
                unmodified_count += 1;
                if (unmodified_count > _max_unmodified_count)
                    break;
            } else {
                std::cout << *_grammer_list[0] << std::endl;
                unmodified_count = 0;
                prev_eval = eval;
            }
        }
    }

    auto update() -> double {
        std::vector<evaluated<std::shared_ptr<grammer>>> evaluated_grammers;
        for (const auto & grm : _grammer_list)
            evaluated_grammers.emplace_back(grm, evaluate(*grm));

        std::sort(std::begin(evaluated_grammers), std::end(evaluated_grammers), [](auto && a, auto && b){
            return a.second > b.second;
        });

        std::vector<evaluated<std::shared_ptr<grammer>>> rankinged_grammers;
        for (std::size_t i = 0; i < evaluated_grammers.size(); ++i)
            rankinged_grammers.emplace_back(evaluated_grammers[i].first, evaluated_grammers.size() - i);

        std::vector<std::shared_ptr<grammer>> next_generation;

        const std::size_t elite_number = static_cast<std::size_t>(std::floor(_elite_ratio * _grammer_list.size()));
        for (std::size_t i = 0; i < elite_number; ++i)
            next_generation.push_back(evaluated_grammers[i].first);

        const std::size_t mutation_number = static_cast<std::size_t>(std::floor(_mutation_ratio * _grammer_list.size()));
        for (std::size_t i = 0; i < mutation_number; ++i) {
            auto clone = select_individual(rankinged_grammers)->clone();
            auto node = random_select_node(clone);
            mutate_node(node);
            next_generation.push_back(clone);
        }

        for (std::size_t i = next_generation.size(); i < _grammer_list.size(); ++i) {
            auto parent_a = select_individual(rankinged_grammers);
            auto parent_b = select_individual(rankinged_grammers);
            next_generation.push_back(create_crossed_tree(parent_a, parent_b).first);
        }

        for (auto & grm : next_generation)
            optimize_tree(grm);

        std::swap(_grammer_list, next_generation);

        double max_evaluation_value = evaluated_grammers[0].second;
        return max_evaluation_value;
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
    auto evaluate(const grammer & grm) const -> double {
        double value = 0;
        for (const auto & input : _input_list)
            value += grm.evaluate(input);
        return value;
    }

    std::vector<std::string> _input_list;
    std::vector<std::shared_ptr<grammer>> _grammer_list;
    double _elite_ratio{};
    double _mutation_ratio{};
    std::size_t _max_unmodified_count{};
    std::set<std::string> _dictionary;
};

} // namespace grammergen

#endif
