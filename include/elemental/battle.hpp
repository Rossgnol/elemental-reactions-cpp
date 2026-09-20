#pragma once

#include "elemental/reaction.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elemental {

enum class AbilityKind {
    skill,
    burst
};

enum class TargetingMode {
    primary,
    all_enemies
};

enum class BattleLogKind {
    character_switch,
    ability_cast,
    attack_damage,
    reaction,
    energy_gain,
    enemy_defeated
};

std::string_view to_string(AbilityKind kind) noexcept;
std::string_view to_string(BattleLogKind kind) noexcept;

// One damage/application instance on an ability timeline. Multiple entries
// model multi-hit and persistent off-field abilities.
struct AbilityHit {
    double delay_seconds{0.0};
    TargetingMode targeting{TargetingMode::primary};
    Element element{Element::physical};
    double gauge_units{0.0};
    double base_damage{0.0};
    double damage_bonus{0.0};
    double critical_multiplier{1.0};
    double defense_ignore{0.0};
    bool blunt{false};

    // When enabled, the ICD group is tracked independently for each target.
    bool uses_standard_icd{false};
    std::string icd_group{};

    // Particles are generated once when this hit reaches at least one living
    // enemy. Element::physical represents a clear (non-elemental) particle.
    double particle_count{0.0};
    Element particle_element{Element::physical};

    // Zero means this hit does not affect Dendro Cores. Use max() for all
    // cores selected by this target-local simulation.
    std::size_t dendro_core_trigger_limit{0};
};

struct AbilityDefinition {
    std::string name{};
    double cooldown_seconds{1.0};
    double energy_cost{0.0};
    double action_duration_seconds{0.0};
    std::vector<AbilityHit> hits{};
};

struct CharacterDefinition {
    ActorStats stats{};
    Element element{Element::physical};

    // 1.0 means 100% Energy Recharge, 1.8 means 180%.
    double energy_recharge{1.0};
    double initial_energy{0.0};
    AbilityDefinition skill{};
    AbilityDefinition burst{};
};

class Character {
public:
    explicit Character(CharacterDefinition definition);

    [[nodiscard]] const CharacterDefinition& definition() const noexcept;
    [[nodiscard]] double energy() const noexcept;
    [[nodiscard]] double maximum_energy() const noexcept;
    [[nodiscard]] double skill_ready_at() const noexcept;
    [[nodiscard]] double burst_ready_at() const noexcept;
    [[nodiscard]] std::size_t skill_cast_count() const noexcept;
    [[nodiscard]] std::size_t burst_cast_count() const noexcept;
    [[nodiscard]] bool skill_ready(double time_seconds) const noexcept;
    [[nodiscard]] bool burst_ready(double time_seconds) const noexcept;

    void set_energy(double energy) noexcept;

private:
    CharacterDefinition definition_{};
    double energy_{0.0};
    double skill_ready_at_{0.0};
    double burst_ready_at_{0.0};
    std::size_t skill_cast_count_{0};
    std::size_t burst_cast_count_{0};
    std::unordered_map<std::string, StandardIcdTracker> icd_by_target_{};

    friend class BattleSimulator;
};

struct EnemyDefinition {
    std::string id{"enemy"};
    double maximum_health{100'000.0};
    TargetStats stats{};
};

class Enemy {
public:
    explicit Enemy(EnemyDefinition definition);

    [[nodiscard]] const EnemyDefinition& definition() const noexcept;
    [[nodiscard]] double health() const noexcept;
    [[nodiscard]] bool defeated() const noexcept;
    [[nodiscard]] double defeated_at() const noexcept;
    [[nodiscard]] TargetState& elemental_state() noexcept;
    [[nodiscard]] const TargetState& elemental_state() const noexcept;

private:
    EnemyDefinition definition_{};
    double health_{0.0};
    double defeated_at_{-1.0};
    TargetState elemental_state_{};

    [[nodiscard]] double take_damage(double damage, double time_seconds) noexcept;

    friend class BattleSimulator;
};

struct BattleConfig {
    double duration_seconds{30.0};
    double switch_cooldown_seconds{1.0};
    double particle_pickup_delay_seconds{0.7};
    double maximum_time_step_seconds{0.05};
    std::size_t initial_active_character{0};
    bool burst_before_skill{true};
    bool stop_when_all_enemies_defeated{true};
    EngineConfig reactions{};
};

struct BattleLogEntry {
    double time_seconds{0.0};
    BattleLogKind kind{BattleLogKind::ability_cast};
    std::string actor_id{};
    std::string target_id{};
    std::string detail{};
    Reaction reaction{Reaction::none};
    double amount{0.0};
};

struct BattleResult {
    double elapsed_seconds{0.0};
    double total_damage{0.0};
    std::size_t enemies_defeated{0};
    std::vector<BattleLogEntry> log{};
};

// A deterministic four-character auto-battle driver. Skills are cast as soon
// as their cooldown, the shared action lock, and switching allow. Bursts also
// require full Energy. burst_before_skill resolves a same-time conflict.
class BattleSimulator {
public:
    BattleSimulator(
        std::vector<Character> party,
        std::vector<Enemy> enemies,
        BattleConfig config = {});

    [[nodiscard]] BattleResult run();

    [[nodiscard]] const std::vector<Character>& party() const noexcept;
    [[nodiscard]] const std::vector<Enemy>& enemies() const noexcept;
    [[nodiscard]] std::size_t active_character_index() const noexcept;

private:
    std::vector<Character> party_{};
    std::vector<Enemy> enemies_{};
    BattleConfig config_{};
    ReactionEngine reaction_engine_{};
    std::size_t active_character_index_{0};
    bool has_run_{false};
};

} // namespace elemental
