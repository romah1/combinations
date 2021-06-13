#include "Combinations.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <variant>

bool Combinations::load(const std::filesystem::path & resource)
{
    pugi::xml_document combinations_xml;
    pugi::xml_parse_result result = combinations_xml.load_file(resource.c_str());
    if (!result) {
        return false;
    }
    pugi::xml_node root = combinations_xml.document_element();
    combinations.reserve(root.select_nodes("combination").size());
    for (const auto & combination : root.children()) {
        combinations.emplace_back(Combination::from_xml_combination(combination));
    }
    return true;
}

std::string Combinations::classify(const std::vector<Component> & components, std::vector<int> & order) const
{
    if (!components.empty()) {
        for (const auto & combination : combinations) {
            if ((combination.cardinality == "fixed" && check_fits_fixed(components, combination.legs, order)) ||
                (combination.cardinality == "more" && check_fits_more(components, combination, order)) ||
                (combination.cardinality == "multiple" && check_fits_multiple(components, combination.legs, order))) {
                return combination.name;
            }
        }
    }
    order.clear();
    return "Unclassified";
}
bool Combinations::check_fits_multiple(const std::vector<Component> & components, const std::vector<Leg> & legs, std::vector<int> & order)
{
    if (components.size() % legs.size() == 0) {
        std::vector<std::pair<Component, int>> components_paired;
        for (std::size_t i = 0; i < components.size(); i++) {
            components_paired.emplace_back(components[i], i + 1);
        }
        if (check_fits_multiple(components_paired, legs, order)) {
            return true;
        }
    }
    return false;
}
bool Combinations::check_fits_multiple(std::vector<std::pair<Component, int>> & sorted_components, const std::vector<Leg> & legs, std::vector<int> & order)
{
    sorted_components.push_back(sorted_components[0]);
    sorted_components.pop_back();
    //    auto sorted_components = sorted_components;
    auto cnt_groups = sorted_components.size() / legs.size();
    auto comp = [](const auto & c1, const auto & c2) {
        return tm_more(c2.first.expiration, c1.first.expiration);
    };
    std::sort(sorted_components.begin(), sorted_components.end(), comp);
    do {
        bool every_fits = true;
        std::vector<std::pair<Component, Leg>> paired(sorted_components.size());
        for (std::size_t g = 0; g < cnt_groups; g++) {
            paired = pair_vectors(sorted_components, legs, g * legs.size());
            if (!check_all(paired)) {
                every_fits = false;
                break;
            }
        }
        if (every_fits) {
            order.resize(sorted_components.size());
            for (std::size_t k = 0; k < sorted_components.size(); k++) {
                order[sorted_components[k].second - 1] = static_cast<int>(k) + 1;
            }
            return true;
        }
    } while (std::next_permutation(sorted_components.begin(), sorted_components.end(), comp));
    return false;
}
bool Combinations::check_fits_fixed(const std::vector<Component> & components, const std::vector<Leg> & legs, std::vector<int> & order)
{
    if (components.size() == legs.size()) {
        std::vector<std::pair<Component, int>> components_paired;
        for (std::size_t i = 0; i < components.size(); i++) {
            components_paired.emplace_back(components[i], i + 1);
        }
        if (check_fits_fixed(components_paired, legs, order)) {
            return true;
        }
    }
    return false;
}
bool Combinations::check_fits_fixed(std::vector<std::pair<Component, int>> & components, const std::vector<Leg> & legs, std::vector<int> & order)
{
    components.push_back(components[0]);
    components.pop_back();
    //    auto components = components;
    auto comp = [](const auto & c1, const auto & c2) {
        return c1.second < c2.second;
    };
    std::sort(components.begin(), components.end(), comp);
    do {
        auto paired = pair_vectors(components, legs, 0);
        if (check_all(paired)) {
            order.resize(components.size());
            for (std::size_t k = 0; k < components.size(); k++) {
                order[components[k].second - 1] = static_cast<int>(k) + 1;
            }
            return true;
        }
    } while (std::next_permutation(components.begin(), components.end(), comp));

    return false;
}
bool Combinations::check_fits_more(const std::vector<Component> & components, const Combination & combination, std::vector<int> & order)
{
    if (components.size() >= combination.mincount && check_fits_more(components, combination.legs[0])) {
        order.resize(components.size());
        for (std::size_t i = 0; i < order.size(); i++) {
            order[i] = static_cast<int>(i + 1);
        }
        return true;
    }
    return false;
}
bool Combinations::check_fits_more(const std::vector<Component> & components, const Combinations::Leg & leg)
{
    for (const auto & component : components) {
        if (!type_comparator(component.type, leg.type)) {
            return false;
        }
        if (std::holds_alternative<double>(leg.ratio)) {
            if (component.ratio != std::get<double>(leg.ratio)) {
                return false;
            }
        }
        else {
            auto c = std::get<char>(leg.ratio);
            if ((c == '+' && component.ratio < 0) || (c == '-' && component.ratio > 0)) {
                return false;
            }
        }
    }
    return true;
}

bool Combinations::check_all(std::vector<std::pair<Component, Leg>> & match)
{
    return check_ratio_equivalence(match) && check_strike_order(match) && check_expiration_order(match);
}

bool Combinations::check_strike_order(std::vector<std::pair<Component, Leg>> & sorted_match)
{
    sorted_match.push_back(sorted_match[0]);
    sorted_match.pop_back();
    std::sort(sorted_match.begin(), sorted_match.end(), [](const auto & p1, const auto & p2) -> bool {
        if (std::holds_alternative<char>(p1.second.strike) && std::holds_alternative<char>(p2.second.strike)) {
            return std::get<char>(p1.second.strike) < std::get<char>(p2.second.strike);
        }
        else {
            auto pr1 = std::holds_alternative<int>(p1.second.strike) ? std::get<int>(p1.second.strike) : 0;
            auto pr2 = std::holds_alternative<int>(p2.second.strike) ? std::get<int>(p2.second.strike) : 0;
            return pr1 < pr2;
        }
    });
    std::size_t cur_i = 0;
    while (cur_i < sorted_match.size() - 1) {
        auto next_i = cur_i + 1;
        if (!is_option(sorted_match[cur_i].second.type)) {
            cur_i++;
            continue;
        }
        while (next_i < sorted_match.size() && !is_option(sorted_match[next_i].second.type)) {
            next_i++;
        }
        if (next_i == sorted_match.size()) {
            break;
        }
        if (std::holds_alternative<char>(sorted_match[cur_i].second.strike) &&
            std::holds_alternative<char>(sorted_match[next_i].second.strike)) {
            if (std::get<char>(sorted_match[cur_i].second.strike) == std::get<char>(sorted_match[next_i].second.strike) &&
                (sorted_match[cur_i].first.strike != sorted_match[next_i].first.strike)) {
                return false;
            }
        }
        else {
            auto pr1 = std::holds_alternative<int>(sorted_match[cur_i].second.strike) ? std::get<int>(sorted_match[cur_i].second.strike) : 0;
            auto pr2 = std::holds_alternative<int>(sorted_match[next_i].second.strike) ? std::get<int>(sorted_match[next_i].second.strike) : 0;
            if (pr1 == pr2) {
                if (sorted_match[cur_i].first.strike != sorted_match[next_i].first.strike) {
                    return false;
                }
            }
            else {
                if (sorted_match[cur_i].first.strike >= sorted_match[next_i].first.strike) {
                    return false;
                }
            }
        }
        cur_i = next_i;
    }
    return true;
}
bool Combinations::check_expiration_order(std::vector<std::pair<Component, Leg>> & sorted_match)
{
    sorted_match.push_back(sorted_match[0]);
    sorted_match.pop_back();
    std::sort(sorted_match.begin(), sorted_match.end(), [](const auto & p1, const auto & p2) -> bool {
        if (std::holds_alternative<char>(p1.second.expiration) && std::holds_alternative<char>(p2.second.expiration)) {
            return std::get<char>(p1.second.expiration) < std::get<char>(p2.second.expiration);
        }
        else {
            auto pr1 = std::holds_alternative<int>(p1.second.expiration) ? std::get<int>(p1.second.expiration) : 0;
            auto pr2 = std::holds_alternative<int>(p2.second.expiration) ? std::get<int>(p2.second.expiration) : 0;
            return pr1 < pr2;
        }
    });
    for (unsigned long i = 0; i < sorted_match.size() - 1; i++) {
        if (std::holds_alternative<std::pair<int, char>>(sorted_match[i + 1].second.expiration)) {
            auto c = std::get<std::pair<int, char>>(sorted_match[i + 1].second.expiration);
            if ((c.second == 'd' && sorted_match[i + 1].first.expiration.tm_mday - sorted_match[0].first.expiration.tm_mday != c.first) ||
                (c.second == 'm' &&
                 sorted_match[i + 1].first.expiration.tm_mon - sorted_match[0].first.expiration.tm_mon != c.first) ||
                (c.second == 'q' &&
                 sorted_match[i + 1].first.expiration.tm_mon - sorted_match[0].first.expiration.tm_mon != 3 * c.first) ||
                (c.second == 'y' &&
                 sorted_match[i + 1].first.expiration.tm_year - sorted_match[0].first.expiration.tm_year != c.first)) {
                return false;
            }
        }
        else if (std::holds_alternative<char>(sorted_match[i].second.expiration) &&
                 std::holds_alternative<char>(sorted_match[i + 1].second.expiration)) {
            if (std::get<char>(sorted_match[i].second.expiration) == std::get<char>(sorted_match[i + 1].second.expiration) &&
                (!tm_equal(sorted_match[i].first.expiration, sorted_match[i + 1].first.expiration))) {
                return false;
            }
        }
        else {
            auto pr1 = std::holds_alternative<int>(sorted_match[i].second.expiration) ? std::get<int>(sorted_match[i].second.expiration) : 0;
            auto pr2 = std::holds_alternative<int>(sorted_match[i + 1].second.expiration) ? std::get<int>(sorted_match[i + 1].second.expiration) : 0;
            if (pr1 == pr2) {
                if (!tm_equal(sorted_match[i].first.expiration, sorted_match[i + 1].first.expiration)) {
                    return false;
                }
            }
            else {
                if (!tm_more(sorted_match[i + 1].first.expiration, sorted_match[i].first.expiration)) {
                    return false;
                }
            }
        }
    }
    return true;
}
bool Combinations::check_ratio_equivalence(const std::vector<std::pair<Component, Leg>> & match)
{
    for (const auto & cur_match : match) {
        if (cur_match.first.type != cur_match.second.type) {
            return false;
        }
        if (std::holds_alternative<double>(cur_match.second.ratio)) {
            if (cur_match.first.ratio != std::get<double>(cur_match.second.ratio)) {
                return false;
            }
        }
        else {
            auto & c = std::get<char>(cur_match.second.ratio);
            if ((c == '+' && cur_match.first.ratio < 0) || (c == '-' && cur_match.first.ratio > 0)) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::pair<Component, Combinations::Leg>> Combinations::pair_vectors(const std::vector<std::pair<Component, int>> & components, const std::vector<Leg> & legs, std::size_t from)
{
    std::vector<std::pair<Component, Leg>> res;
    for (unsigned long i = 0; i < legs.size(); i++) {
        res.emplace_back(components[from + i].first, legs[i]);
    }
    return res;
}

bool Combinations::tm_equal(const tm & t1, const tm & t2)
{
    return t1.tm_year == t2.tm_year && t1.tm_mon == t2.tm_mon && t1.tm_mday == t2.tm_mday;
}

bool Combinations::tm_more(const tm & t1, const tm & t2)
{
    if (t1.tm_year == t2.tm_year) {
        if (t1.tm_mon == t2.tm_mon) {
            return t1.tm_mday > t2.tm_mday;
        }
        else {
            return t1.tm_mon > t2.tm_mon;
        }
    }
    else {
        return t1.tm_year > t2.tm_year;
    }
}
bool Combinations::is_option(const InstrumentType & i)
{
    if (i == InstrumentType::O || i == InstrumentType::P || i == InstrumentType::C) {
        return true;
    }
    return false;
}
bool Combinations::type_comparator(const InstrumentType & i1, const InstrumentType & i2)
{
    if (i1 == i2 || (is_option(i1) && is_option(i2))) {
        return true;
    }
    return false;
}

Combinations::Leg Combinations::Leg::from_xml_leg(const pugi::xml_node & leg_node)
{
    Leg leg;
    for (const auto & attribute : leg_node.attributes()) {
        if (strcmp(attribute.name(), "type") == 0) {
            leg.type = static_cast<InstrumentType>(leg_node.attribute("type").as_string()[0]);
        }
        else if (strcmp(attribute.name(), "ratio") == 0) {
            std::string str_data = leg_node.attribute("ratio").as_string();
            if ((str_data.length() == 1 && str_data[0] != '-' && str_data[0] != '+') || str_data.length() > 1) {
                leg.ratio = leg_node.attribute("ratio").as_double();
            }
            else {
                leg.ratio = str_data[0];
            }
        }
        else if (strcmp(attribute.name(), "strike") == 0) {
            leg.strike = leg_node.attribute("strike").as_string()[0];
        }
        else if (strcmp(attribute.name(), "strike_offset") == 0) {
            std::string data = leg_node.attribute("strike_offset").as_string();
            leg.strike = static_cast<int>(data.length());
            if (data[0] == '-') {
                leg.strike = -std::get<int>(leg.strike);
            }
        }
        else if (strcmp(attribute.name(), "expiration") == 0) {
            leg.expiration = leg_node.attribute("expiration").as_string()[0];
        }
        else if (strcmp(attribute.name(), "expiration_offset") == 0) {
            std::string data = leg_node.attribute("expiration_offset").as_string();
            auto len = data.length();
            if (data[len - 1] == 'd' || data[len - 1] == 'm' || data[len - 1] == 'q' || data[len - 1] == 'y') {
                long cnt;
                if (len == 1) {
                    cnt = 1;
                }
                else {
                    try {
                        cnt = std::stoi(data.substr(0, len - 1));
                    }
                    catch (std::invalid_argument & e) {
                        std::cerr << "Caught Invalid Argument Exception\n";
                    }
                }
                leg.expiration = std::make_pair(cnt, data[len - 1]);
            }
            else {
                leg.expiration = static_cast<int>(len);
                if (data[0] == '-') {
                    leg.expiration = -std::get<int>(leg.expiration);
                }
            }
        }
    }
    return leg;
}
std::vector<Combinations::Leg> Combinations::Leg::from_xml_legs(const pugi::xml_node & legs_node)
{
    std::vector<Leg> result;
    result.reserve(legs_node.select_nodes("leg").size());
    for (const auto & leg : legs_node.children()) {
        result.emplace_back(from_xml_leg(leg));
    }
    return result;
}
Combinations::Combination Combinations::Combination::from_xml_combination(const pugi::xml_node & combination_node)
{
    Combination combination;
    for (const auto & attribute : combination_node.attributes()) {
        if (strcmp(attribute.name(), "name") == 0) {
            combination.name = combination_node.attribute("name").as_string();
        }
        else if (strcmp(attribute.name(), "shortname") == 0) {
            combination.shortname = combination_node.attribute("shortname").as_string();
        }
        else if (strcmp(attribute.name(), "identifier") == 0) {
            combination.identifier = combination_node.attribute("identifier").as_string();
        }
    }
    combination.cardinality = combination_node.child("legs").attribute("cardinality").as_string();
    if (combination.cardinality == "more") {
        combination.mincount = combination_node.child("legs").attribute("mincount").as_int();
    }
    combination.legs = Leg::from_xml_legs(combination_node.child("legs"));
    return combination;
}
