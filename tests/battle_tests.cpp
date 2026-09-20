#include "elemental/battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}

void near(double actual, double expected, double tolerance, std::string_view message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << "[FAIL] " << message << ": expected " << expected << ", got " << actual
                  << '\n';
        ++failures;
    }
}

elemental::CharacterDefinition character_definition(
    std::string id,
    elemental::Element element,
    double skill_cooldown = 100.0) {
    elemental::CharacterDefinition value;
    value.stats.id = std::move(id);
    value.stats.level = 90;
    value.element = element;
    value.energy_recharge = 1.0;
    value.skill.name = "skill";
    value.skill.cooldown_seconds = skill_cooldown;
    value.skill.action_duration_seconds = 0.10;
    value.burst.name = "burst";
    value.burst.cooldown_seconds = 100.0;
    value.burst.energy_cost = 100.0;
    value.burst.action_duration_seconds = 0.10;
    return value;
}

std::vector<elemental::Enemy> enemies(std::size_t count, double health = 100'000.0) {
    std::vector<elemental::Enemy> result;
    for (std::size_t i = 0; i < count; ++i) {
        elemental::EnemyDefinition value;
        value.id = "enemy-" + std::to_string(i);
        value.maximum_health = health;
        value.stats.level = 90;
        result.emplace_back(std::move(value));
    }
    return result;
}

elemental::BattleConfig short_config(double duration) {
    elemental::BattleConfig value;
    value.duration_seconds = duration;
    value.switch_cooldown_seconds = 0.0;
    value.particle_pickup_delay_seconds = 0.02;
    value.maximum_time_step_seconds = 0.01;
    value.stop_when_all_enemies_defeated = false;
    return value;
}

void particle_energy_distribution() {
    auto first = character_definition("active-pyro", elemental::Element::pyro);
    first.energy_recharge = 1.5;
    elemental::AbilityHit particle_hit;
    particle_hit.element = elemental::Element::pyro;
    particle_hit.gauge_units = 1.0;
    particle_hit.particle_count = 1.0;
    particle_hit.particle_element = elemental::Element::pyro;
    first.skill.hits.push_back(particle_hit);

    auto second = character_definition("off-pyro", elemental::Element::pyro);
    second.energy_recharge = 2.0;
    auto third = character_definition("off-hydro", elemental::Element::hydro);
    auto fourth = character_definition("off-anemo", elemental::Element::anemo);

    std::vector<elemental::Character> party;
    party.emplace_back(std::move(first));
    party.emplace_back(std::move(second));
    party.emplace_back(std::move(third));
    party.emplace_back(std::move(fourth));

    elemental::BattleSimulator simulator(
        std::move(party), enemies(1), short_config(0.08));
    (void)simulator.run();

    near(simulator.party()[0].energy(), 4.5, 1.0e-9, "active matching particle Energy");
    near(simulator.party()[1].energy(), 3.6, 1.0e-9, "off-field matching Energy");
    near(simulator.party()[2].energy(), 0.6, 1.0e-9, "off-field different Energy");
    near(simulator.party()[3].energy(), 0.6, 1.0e-9, "second off-field different Energy");
}

void skills_recast_when_cooldown_ends() {
    auto first = character_definition("first", elemental::Element::pyro, 1.0);
    first.skill.action_duration_seconds = 0.01;
    auto second = character_definition("second", elemental::Element::hydro);
    second.skill.action_duration_seconds = 0.01;
    auto third = character_definition("third", elemental::Element::cryo);
    third.skill.action_duration_seconds = 0.01;
    auto fourth = character_definition("fourth", elemental::Element::anemo);
    fourth.skill.action_duration_seconds = 0.01;

    std::vector<elemental::Character> party;
    party.emplace_back(std::move(first));
    party.emplace_back(std::move(second));
    party.emplace_back(std::move(third));
    party.emplace_back(std::move(fourth));

    elemental::BattleSimulator simulator(
        std::move(party), enemies(1), short_config(3.05));
    (void)simulator.run();
    check(
        simulator.party()[0].skill_cast_count() == 4,
        "a ready Skill should be recast at 0, 1, 2, and 3 seconds");
    check(
        simulator.party()[1].skill_cast_count() == 1 &&
            simulator.party()[2].skill_cast_count() == 1 &&
            simulator.party()[3].skill_cast_count() == 1,
        "round-robin should let every initially ready character act");
}

void burst_requires_energy_and_cooldown() {
    auto first = character_definition("battery", elemental::Element::pyro);
    first.burst.energy_cost = 3.0;
    first.burst.cooldown_seconds = 10.0;
    elemental::AbilityHit particle_hit;
    particle_hit.element = elemental::Element::pyro;
    particle_hit.particle_count = 1.0;
    particle_hit.particle_element = elemental::Element::pyro;
    first.skill.hits.push_back(particle_hit);

    std::vector<elemental::Character> party;
    party.emplace_back(std::move(first));
    party.emplace_back(character_definition("second", elemental::Element::hydro));
    party.emplace_back(character_definition("third", elemental::Element::cryo));
    party.emplace_back(character_definition("fourth", elemental::Element::anemo));

    elemental::BattleSimulator simulator(
        std::move(party), enemies(1), short_config(0.25));
    (void)simulator.run();
    check(simulator.party()[0].burst_cast_count() == 1, "full Energy should unlock Burst");
    near(simulator.party()[0].energy(), 0.0, 1.0e-9, "Burst should consume its Energy cost");
    near(simulator.party()[0].burst_ready_at(), 10.1, 1.0e-9, "Burst cooldown starts on cast");
}

void all_enemy_targeting_and_damage() {
    auto first = character_definition("aoe", elemental::Element::pyro);
    first.skill.action_duration_seconds = 1.0;
    elemental::AbilityHit aoe;
    aoe.element = elemental::Element::pyro;
    aoe.base_damage = 1'000.0;
    aoe.targeting = elemental::TargetingMode::all_enemies;
    first.skill.hits.push_back(aoe);

    std::vector<elemental::Character> party;
    party.emplace_back(std::move(first));
    party.emplace_back(character_definition("second", elemental::Element::hydro));
    party.emplace_back(character_definition("third", elemental::Element::cryo));
    party.emplace_back(character_definition("fourth", elemental::Element::anemo));

    elemental::BattleSimulator simulator(
        std::move(party), enemies(2), short_config(0.05));
    const auto result = simulator.run();
    near(result.total_damage, 1'000.0, 1.0e-6, "AoE should damage both equal-level enemies");
    near(simulator.enemies()[0].health(), 99'500.0, 1.0e-6, "first enemy AoE damage");
    near(simulator.enemies()[1].health(), 99'500.0, 1.0e-6, "second enemy AoE damage");
}

void reactions_are_independent_per_target() {
    auto hydro = character_definition("hydro", elemental::Element::hydro);
    hydro.skill.action_duration_seconds = 0.01;
    elemental::AbilityHit wet;
    wet.element = elemental::Element::hydro;
    wet.gauge_units = 2.0;
    wet.targeting = elemental::TargetingMode::all_enemies;
    hydro.skill.hits.push_back(wet);

    auto pyro = character_definition("pyro", elemental::Element::pyro);
    pyro.skill.action_duration_seconds = 1.0;
    elemental::AbilityHit fire;
    fire.element = elemental::Element::pyro;
    fire.gauge_units = 1.0;
    fire.base_damage = 1'000.0;
    fire.targeting = elemental::TargetingMode::all_enemies;
    pyro.skill.hits.push_back(fire);

    std::vector<elemental::Character> party;
    party.emplace_back(std::move(hydro));
    party.emplace_back(std::move(pyro));
    party.emplace_back(character_definition("third", elemental::Element::cryo));
    party.emplace_back(character_definition("fourth", elemental::Element::anemo));

    elemental::BattleSimulator simulator(
        std::move(party), enemies(2), short_config(0.05));
    const auto result = simulator.run();
    const auto vaporize_count = std::count_if(
        result.log.begin(), result.log.end(), [](const elemental::BattleLogEntry& entry) {
            return entry.reaction == elemental::Reaction::vaporize;
        });
    check(vaporize_count == 2, "each enemy should resolve its own Vaporize");
    near(result.total_damage, 1'500.0, 1.0e-6, "reverse Vaporize damage on two enemies");
}

} // namespace

int main() {
    particle_energy_distribution();
    skills_recast_when_cooldown_ends();
    burst_requires_energy_and_cooldown();
    all_enemy_targeting_and_damage();
    reactions_are_independent_per_target();

    if (failures == 0) {
        std::cout << "All battle simulator tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
