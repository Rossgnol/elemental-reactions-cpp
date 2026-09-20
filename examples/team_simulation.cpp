#include "elemental/battle.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using elemental::AbilityDefinition;
using elemental::AbilityHit;
using elemental::CharacterDefinition;
using elemental::Element;
using elemental::Reaction;
using elemental::TargetingMode;

AbilityHit hit(
    double delay,
    Element element,
    double gauge,
    double base_damage,
    TargetingMode targeting = TargetingMode::primary) {
    AbilityHit value;
    value.delay_seconds = delay;
    value.element = element;
    value.gauge_units = gauge;
    value.base_damage = base_damage;
    value.targeting = targeting;
    return value;
}

void add_repeating_hits(
    AbilityDefinition& ability,
    double first_delay,
    double interval,
    int count,
    Element element,
    double gauge,
    double base_damage,
    TargetingMode targeting,
    bool standard_icd,
    std::string_view icd_group) {
    for (int i = 0; i < count; ++i) {
        auto value = hit(
            first_delay + interval * static_cast<double>(i),
            element,
            gauge,
            base_damage,
            targeting);
        value.uses_standard_icd = standard_icd;
        value.icd_group = std::string(icd_group);
        ability.hits.push_back(std::move(value));
    }
}

CharacterDefinition xiangling() {
    CharacterDefinition value;
    value.stats.id = "Xiangling";
    value.stats.level = 90;
    value.stats.elemental_mastery = 220.0;
    value.stats.set_bonus(Reaction::vaporize, 0.15);
    value.element = Element::pyro;
    value.energy_recharge = 2.00;
    value.initial_energy = 80.0;

    value.skill.name = "Guoba Attack";
    value.skill.cooldown_seconds = 12.0;
    value.skill.action_duration_seconds = 0.45;
    for (int i = 0; i < 4; ++i) {
        auto flame = hit(
            2.0 + 1.5 * static_cast<double>(i),
            Element::pyro,
            1.0,
            3'400.0,
            TargetingMode::all_enemies);
        flame.particle_count = 1.0;
        flame.particle_element = Element::pyro;
        flame.dendro_core_trigger_limit = 2;
        value.skill.hits.push_back(std::move(flame));
    }

    value.burst.name = "Pyronado";
    value.burst.cooldown_seconds = 20.0;
    value.burst.energy_cost = 80.0;
    value.burst.action_duration_seconds = 1.10;
    value.burst.hits.push_back(
        hit(0.20, Element::pyro, 1.0, 2'000.0, TargetingMode::all_enemies));
    value.burst.hits.push_back(
        hit(0.42, Element::pyro, 1.0, 2'400.0, TargetingMode::all_enemies));
    value.burst.hits.push_back(
        hit(0.68, Element::pyro, 1.0, 3'000.0, TargetingMode::all_enemies));
    add_repeating_hits(
        value.burst,
        1.40,
        1.20,
        8,
        Element::pyro,
        1.0,
        6'800.0,
        TargetingMode::all_enemies,
        false,
        "pyronado");
    for (std::size_t i = 3; i < value.burst.hits.size(); ++i) {
        value.burst.hits[i].dendro_core_trigger_limit = 2;
    }
    return value;
}

CharacterDefinition xingqiu() {
    CharacterDefinition value;
    value.stats.id = "Xingqiu";
    value.stats.level = 90;
    value.stats.elemental_mastery = 80.0;
    value.element = Element::hydro;
    value.energy_recharge = 2.00;
    value.initial_energy = 80.0;

    value.skill.name = "Fatal Rainscreen";
    value.skill.cooldown_seconds = 21.0;
    value.skill.action_duration_seconds = 0.80;
    value.skill.hits.push_back(hit(0.20, Element::hydro, 1.0, 4'200.0));
    auto second = hit(0.48, Element::hydro, 1.0, 5'000.0);
    second.particle_count = 5.0;
    second.particle_element = Element::hydro;
    value.skill.hits.push_back(std::move(second));

    value.burst.name = "Raincutter";
    value.burst.cooldown_seconds = 20.0;
    value.burst.energy_cost = 80.0;
    value.burst.action_duration_seconds = 0.80;
    // The demo assumes one Normal Attack trigger per second. Three scheduled
    // sword hits per wave preserve the common per-target 3-hit ICD behavior.
    for (int wave = 0; wave < 15; ++wave) {
        for (int sword = 0; sword < 3; ++sword) {
            auto rain_sword = hit(
                1.0 + static_cast<double>(wave) + 0.01 * static_cast<double>(sword),
                Element::hydro,
                1.0,
                1'900.0);
            rain_sword.uses_standard_icd = true;
            rain_sword.icd_group = "raincutter";
            value.burst.hits.push_back(std::move(rain_sword));
        }
    }
    return value;
}

CharacterDefinition kaeya() {
    CharacterDefinition value;
    value.stats.id = "Kaeya";
    value.stats.level = 90;
    value.stats.elemental_mastery = 120.0;
    value.element = Element::cryo;
    value.energy_recharge = 1.60;
    value.initial_energy = 60.0;

    value.skill.name = "Frostgnaw";
    value.skill.cooldown_seconds = 6.0;
    value.skill.action_duration_seconds = 0.55;
    auto frostgnaw = hit(
        0.25, Element::cryo, 1.0, 5'200.0, TargetingMode::all_enemies);
    frostgnaw.particle_count = 2.5;
    frostgnaw.particle_element = Element::cryo;
    value.skill.hits.push_back(std::move(frostgnaw));

    value.burst.name = "Glacial Waltz";
    value.burst.cooldown_seconds = 15.0;
    value.burst.energy_cost = 60.0;
    value.burst.action_duration_seconds = 0.85;
    add_repeating_hits(
        value.burst,
        0.50,
        0.50,
        16,
        Element::cryo,
        1.0,
        1'700.0,
        TargetingMode::primary,
        true,
        "glacial-waltz");
    return value;
}

CharacterDefinition sucrose() {
    CharacterDefinition value;
    value.stats.id = "Sucrose";
    value.stats.level = 90;
    value.stats.elemental_mastery = 700.0;
    value.stats.set_bonus(Reaction::swirl, 0.20);
    value.element = Element::anemo;
    value.energy_recharge = 1.60;
    value.initial_energy = 80.0;

    value.skill.name = "Astable Anemohypostasis Creation - 6308";
    value.skill.cooldown_seconds = 15.0;
    value.skill.action_duration_seconds = 0.70;
    auto skill_hit = hit(
        0.35, Element::anemo, 1.0, 3'800.0, TargetingMode::all_enemies);
    skill_hit.particle_count = 4.0;
    skill_hit.particle_element = Element::anemo;
    value.skill.hits.push_back(std::move(skill_hit));

    value.burst.name = "Forbidden Creation - Isomer 75 / Type II";
    value.burst.cooldown_seconds = 20.0;
    value.burst.energy_cost = 80.0;
    value.burst.action_duration_seconds = 1.00;
    add_repeating_hits(
        value.burst,
        0.75,
        2.00,
        3,
        Element::anemo,
        1.0,
        4'500.0,
        TargetingMode::all_enemies,
        true,
        "sucrose-burst");
    return value;
}

CharacterDefinition nahida() {
    CharacterDefinition value;
    value.stats.id = "Nahida";
    value.stats.level = 90;
    value.stats.elemental_mastery = 800.0;
    value.stats.set_bonus(Reaction::bloom, 0.20);
    value.stats.set_bonus(Reaction::spread, 0.20);
    value.element = Element::dendro;
    value.energy_recharge = 1.30;
    value.initial_energy = 50.0;

    value.skill.name = "All Schemes to Know (Press)";
    value.skill.cooldown_seconds = 5.0;
    value.skill.action_duration_seconds = 0.45;
    value.skill.hits.push_back(
        hit(0.25, Element::dendro, 1.5, 4'800.0, TargetingMode::all_enemies));
    // Tri-Karma is reaction-triggered in the game. The deterministic demo
    // schedules one proc before the next press instead of modelling marks.
    auto tri_karma = hit(
        2.75, Element::dendro, 1.5, 6'500.0, TargetingMode::all_enemies);
    tri_karma.particle_count = 3.0;
    tri_karma.particle_element = Element::dendro;
    value.skill.hits.push_back(std::move(tri_karma));

    value.burst.name = "Illusory Heart";
    value.burst.cooldown_seconds = 13.5;
    value.burst.energy_cost = 50.0;
    value.burst.action_duration_seconds = 1.90;
    return value;
}

CharacterDefinition kuki_shinobu() {
    CharacterDefinition value;
    value.stats.id = "KukiShinobu";
    value.stats.level = 90;
    value.stats.elemental_mastery = 900.0;
    value.stats.set_bonus(Reaction::hyperbloom, 0.40);
    value.element = Element::electro;
    value.energy_recharge = 1.40;
    value.initial_energy = 60.0;

    value.skill.name = "Sanctifying Ring";
    value.skill.cooldown_seconds = 15.0;
    value.skill.action_duration_seconds = 0.50;
    auto activation = hit(
        0.20, Element::electro, 1.0, 1'200.0, TargetingMode::all_enemies);
    activation.uses_standard_icd = true;
    activation.icd_group = "sanctifying-ring";
    activation.dendro_core_trigger_limit = 2;
    value.skill.hits.push_back(std::move(activation));
    for (int i = 0; i < 8; ++i) {
        auto ring = hit(
            1.50 + 1.50 * static_cast<double>(i),
            Element::electro,
            1.0,
            1'000.0,
            TargetingMode::all_enemies);
        ring.uses_standard_icd = true;
        ring.icd_group = "sanctifying-ring";
        // Kuki has a 45% particle check per pulse. Fractional particles make
        // this deterministic demo use the same expected value.
        ring.particle_count = 0.45;
        ring.particle_element = Element::electro;
        ring.dendro_core_trigger_limit = 2;
        value.skill.hits.push_back(std::move(ring));
    }

    value.burst.name = "Gyoei Narukami Kariyama Rite";
    value.burst.cooldown_seconds = 15.0;
    value.burst.energy_cost = 60.0;
    value.burst.action_duration_seconds = 0.85;
    add_repeating_hits(
        value.burst,
        0.30,
        0.30,
        7,
        Element::electro,
        1.0,
        1'100.0,
        TargetingMode::all_enemies,
        true,
        "kuki-burst");
    for (auto& burst_hit : value.burst.hits) {
        burst_hit.dendro_core_trigger_limit = 2;
    }
    return value;
}

CharacterDefinition yaoyao() {
    CharacterDefinition value;
    value.stats.id = "Yaoyao";
    value.stats.level = 90;
    value.stats.elemental_mastery = 180.0;
    value.element = Element::dendro;
    value.energy_recharge = 2.20;
    value.initial_energy = 80.0;

    value.skill.name = "Raphanus Sky Cluster";
    value.skill.cooldown_seconds = 15.0;
    value.skill.action_duration_seconds = 0.50;
    for (int i = 0; i < 10; ++i) {
        auto radish = hit(
            1.0 + static_cast<double>(i),
            Element::dendro,
            1.0,
            1'300.0,
            TargetingMode::all_enemies);
        radish.uses_standard_icd = true;
        radish.icd_group = "yuegui-skill";
        // Each radish hit has a 50% particle check in the game. KQM reports
        // about 4.5 particles per ten hits, used here as an expected value.
        radish.particle_count = 0.45;
        radish.particle_element = Element::dendro;
        value.skill.hits.push_back(std::move(radish));
    }

    value.burst.name = "Moonjade Descent";
    value.burst.cooldown_seconds = 20.0;
    value.burst.energy_cost = 80.0;
    value.burst.action_duration_seconds = 1.00;
    add_repeating_hits(
        value.burst,
        0.40,
        0.50,
        10,
        Element::dendro,
        1.0,
        1'500.0,
        TargetingMode::all_enemies,
        true,
        "yaoyao-burst");
    return value;
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

CharacterDefinition from_roster(const std::string& name) {
    const std::string key = lowercase(name);
    if (key == "xiangling") {
        return xiangling();
    }
    if (key == "xingqiu") {
        return xingqiu();
    }
    if (key == "kaeya") {
        return kaeya();
    }
    if (key == "sucrose") {
        return sucrose();
    }
    if (key == "nahida") {
        return nahida();
    }
    if (key == "kukishinobu" || key == "kuki") {
        return kuki_shinobu();
    }
    if (key == "yaoyao") {
        return yaoyao();
    }
    throw std::invalid_argument(
        "unknown demo character: " + name +
        " (available: Xiangling, Xingqiu, Kaeya, Sucrose, Nahida, "
        "KukiShinobu, Yaoyao)");
}

struct Options {
    double duration{30.0};
    int enemy_count{3};
    double replay_speed{0.0};
    bool compact{false};
    std::string preset{"classic"};
    std::vector<std::string> names{"Xiangling", "Xingqiu", "Kaeya", "Sucrose"};
};

void print_usage(const char* executable) {
    std::cout << "Usage:\n";
    std::cout << "  " << executable
              << " [duration_seconds] [enemy_count]"
                 " [character1 character2 character3 character4]\n";
    std::cout << "  " << executable
              << " --preset classic|hyperbloom [--duration seconds]"
                 " [--enemies count] [--replay-speed number] [--compact]\n";
    std::cout << "Examples:\n";
    std::cout << "  " << executable
              << " --preset classic --duration 16 --enemies 2 --replay-speed 2\n";
    std::cout << "  " << executable
              << " --preset hyperbloom --duration 18 --enemies 2 --replay-speed 2\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    if (argc == 1) {
        return options;
    }

    if (argv[1][0] != '-') {
        if (argc != 3 && argc != 7) {
            throw std::invalid_argument("invalid positional arguments");
        }
        options.duration = std::stod(argv[1]);
        options.enemy_count = std::stoi(argv[2]);
        options.preset = "custom";
        if (argc == 7) {
            options.names.assign(argv + 3, argv + 7);
        }
        return options;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        if (argument == "--compact") {
            options.compact = true;
            continue;
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value after " + argument);
        }
        const std::string value = argv[++i];
        if (argument == "--preset") {
            options.preset = lowercase(value);
        } else if (argument == "--duration") {
            options.duration = std::stod(value);
        } else if (argument == "--enemies") {
            options.enemy_count = std::stoi(value);
        } else if (argument == "--replay-speed") {
            options.replay_speed = std::stod(value);
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }

    if (options.preset == "classic") {
        options.names = {"Xiangling", "Xingqiu", "Kaeya", "Sucrose"};
    } else if (options.preset == "hyperbloom") {
        options.names = {"Nahida", "Xingqiu", "KukiShinobu", "Yaoyao"};
    } else {
        throw std::invalid_argument("preset must be classic or hyperbloom");
    }
    return options;
}

bool visible_log_kind(elemental::BattleLogKind kind) {
    return kind == elemental::BattleLogKind::character_switch ||
           kind == elemental::BattleLogKind::ability_cast ||
           kind == elemental::BattleLogKind::reaction ||
           kind == elemental::BattleLogKind::enemy_defeated;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        if (options.duration <= 0.0 || options.enemy_count <= 0 ||
            options.replay_speed < 0.0) {
            throw std::invalid_argument("duration and enemy count must be positive");
        }

        std::vector<elemental::Character> party;
        for (const auto& name : options.names) {
            party.emplace_back(from_roster(name));
        }

        std::vector<elemental::Enemy> enemies;
        for (int i = 0; i < options.enemy_count; ++i) {
            elemental::EnemyDefinition definition;
            definition.id = "Enemy-" + std::to_string(i + 1);
            definition.maximum_health = 500'000.0;
            definition.stats.level = 90;
            for (auto& resistance : definition.stats.resistance) {
                resistance = 0.10;
            }
            enemies.emplace_back(std::move(definition));
        }

        elemental::BattleConfig config;
        config.duration_seconds = options.duration;
        config.initial_active_character = 0;
        config.burst_before_skill = true;

        elemental::BattleSimulator simulator(
            std::move(party), std::move(enemies), config);
        const elemental::BattleResult result = simulator.run();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "=== Four-Character Automatic Battle ===\n";
        std::cout << "Preset: " << options.preset << "\nTeam: ";
        for (std::size_t i = 0; i < options.names.size(); ++i) {
            if (i > 0) {
                std::cout << " / ";
            }
            std::cout << options.names[i];
        }
        std::cout << "\nDuration: " << options.duration
                  << "s | Enemies: " << options.enemy_count << '\n';
        std::cout << "Rule: cast E when ready; cast Q only when ready AND Energy is full.\n";
        if (options.compact) {
            std::cout << "Log mode: compact (repeated reactions are folded for recording).\n";
        }
        if (options.preset == "hyperbloom") {
            std::cout << "Focus: Hydro + Dendro creates Bloom Cores; Electro triggers Hyperbloom.\n";
        }
        std::cout << "\n--- Timeline ---\n" << std::flush;

        double replay_time = 0.0;
        std::unordered_map<std::string, double> last_compact_reaction;
        for (const auto& entry : result.log) {
            if (!visible_log_kind(entry.kind)) {
                continue;
            }
            if (options.compact && entry.kind == elemental::BattleLogKind::reaction) {
                if (entry.detail.find("[damage suppressed]") != std::string::npos ||
                    entry.reaction == Reaction::bloom_explosion) {
                    continue;
                }
                const std::string compact_key =
                    entry.target_id + ':' + std::string(elemental::to_string(entry.reaction));
                const auto previous = last_compact_reaction.find(compact_key);
                if (previous != last_compact_reaction.end() &&
                    entry.time_seconds - previous->second < 0.75) {
                    continue;
                }
                last_compact_reaction[compact_key] = entry.time_seconds;
            }
            if (options.replay_speed > 0.0) {
                const double delay = std::max(0.0, entry.time_seconds - replay_time);
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(delay / options.replay_speed));
                replay_time = entry.time_seconds;
            }
            std::cout << '[' << std::setw(6) << entry.time_seconds << "s] "
                      << elemental::to_string(entry.kind) << " | "
                      << entry.actor_id;
            if (!entry.target_id.empty()) {
                std::cout << " -> " << entry.target_id;
            }
            if (!entry.detail.empty()) {
                std::cout << " | " << entry.detail;
            }
            if (entry.amount > 0.0) {
                std::cout << " | " << entry.amount;
            }
            std::cout << '\n' << std::flush;
        }

        std::cout << "\n=== Battle Summary ===\n";
        std::cout << "Elapsed: " << result.elapsed_seconds << "s\n";
        std::cout << "Damage: " << result.total_damage << '\n';
        std::cout << "Enemies defeated: " << result.enemies_defeated << '/'
                  << simulator.enemies().size() << '\n';
        for (const auto& character : simulator.party()) {
            std::cout << character.definition().stats.id
                      << ": E=" << character.skill_cast_count()
                      << ", Q=" << character.burst_cast_count()
                      << ", Energy=" << character.energy() << '/'
                      << character.maximum_energy() << '\n';
        }
        for (const auto& enemy : simulator.enemies()) {
            std::cout << enemy.definition().id << ": HP=" << enemy.health() << '/'
                      << enemy.definition().maximum_health << '\n';
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
}
