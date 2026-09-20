#include "elemental/battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace elemental {
namespace {

constexpr double epsilon = 1.0e-9;

bool finite_nonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

void validate_ability(const AbilityDefinition& ability, AbilityKind kind) {
    if (ability.name.empty()) {
        throw std::invalid_argument("ability name cannot be empty");
    }
    if (!std::isfinite(ability.cooldown_seconds) || ability.cooldown_seconds <= 0.0 ||
        !finite_nonnegative(ability.action_duration_seconds) ||
        !finite_nonnegative(ability.energy_cost)) {
        throw std::invalid_argument("invalid ability timing or Energy cost");
    }
    if (kind == AbilityKind::skill && ability.energy_cost > epsilon) {
        throw std::invalid_argument("an Elemental Skill cannot have an Energy cost");
    }
    for (const auto& hit : ability.hits) {
        if (!finite_nonnegative(hit.delay_seconds) ||
            !finite_nonnegative(hit.gauge_units) ||
            !finite_nonnegative(hit.base_damage) ||
            !finite_nonnegative(hit.critical_multiplier) ||
            !finite_nonnegative(hit.particle_count)) {
            throw std::invalid_argument("invalid ability hit data");
        }
    }
}

enum class ScheduledKind {
    hit,
    particle
};

struct ScheduledEvent {
    double time_seconds{0.0};
    std::uint64_t sequence{0};
    ScheduledKind kind{ScheduledKind::hit};
    std::size_t actor_index{0};
    std::string ability_name{};
    AbilityHit hit{};
    Element particle_element{Element::physical};
    double particle_count{0.0};
};

struct ScheduledLater {
    bool operator()(const ScheduledEvent& lhs, const ScheduledEvent& rhs) const noexcept {
        if (lhs.time_seconds != rhs.time_seconds) {
            return lhs.time_seconds > rhs.time_seconds;
        }
        return lhs.sequence > rhs.sequence;
    }
};

struct ActionCandidate {
    std::size_t actor_index{0};
    AbilityKind kind{AbilityKind::skill};
};

} // namespace

std::string_view to_string(AbilityKind kind) noexcept {
    switch (kind) {
    case AbilityKind::skill:
        return "Elemental Skill";
    case AbilityKind::burst:
        return "Elemental Burst";
    }
    return "Unknown Ability";
}

std::string_view to_string(BattleLogKind kind) noexcept {
    switch (kind) {
    case BattleLogKind::character_switch:
        return "Switch";
    case BattleLogKind::ability_cast:
        return "Cast";
    case BattleLogKind::attack_damage:
        return "Attack";
    case BattleLogKind::reaction:
        return "Reaction";
    case BattleLogKind::energy_gain:
        return "Energy";
    case BattleLogKind::enemy_defeated:
        return "Defeated";
    }
    return "Unknown Log";
}

Character::Character(CharacterDefinition definition) : definition_(std::move(definition)) {
    if (definition_.stats.id.empty()) {
        throw std::invalid_argument("character id cannot be empty");
    }
    if (!std::isfinite(definition_.energy_recharge) || definition_.energy_recharge < 0.0 ||
        !finite_nonnegative(definition_.initial_energy)) {
        throw std::invalid_argument("invalid character Energy data");
    }
    validate_ability(definition_.skill, AbilityKind::skill);
    validate_ability(definition_.burst, AbilityKind::burst);
    set_energy(definition_.initial_energy);
}

const CharacterDefinition& Character::definition() const noexcept {
    return definition_;
}

double Character::energy() const noexcept {
    return energy_;
}

double Character::maximum_energy() const noexcept {
    return definition_.burst.energy_cost;
}

double Character::skill_ready_at() const noexcept {
    return skill_ready_at_;
}

double Character::burst_ready_at() const noexcept {
    return burst_ready_at_;
}

std::size_t Character::skill_cast_count() const noexcept {
    return skill_cast_count_;
}

std::size_t Character::burst_cast_count() const noexcept {
    return burst_cast_count_;
}

bool Character::skill_ready(double time_seconds) const noexcept {
    return time_seconds + epsilon >= skill_ready_at_;
}

bool Character::burst_ready(double time_seconds) const noexcept {
    return time_seconds + epsilon >= burst_ready_at_ &&
           energy_ + epsilon >= definition_.burst.energy_cost;
}

void Character::set_energy(double energy) noexcept {
    energy_ = std::clamp(energy, 0.0, maximum_energy());
}

Enemy::Enemy(EnemyDefinition definition)
    : definition_(std::move(definition)),
      health_(definition_.maximum_health),
      elemental_state_(definition_.stats) {
    if (definition_.id.empty() || !std::isfinite(definition_.maximum_health) ||
        definition_.maximum_health <= 0.0) {
        throw std::invalid_argument("invalid enemy definition");
    }
}

const EnemyDefinition& Enemy::definition() const noexcept {
    return definition_;
}

double Enemy::health() const noexcept {
    return health_;
}

bool Enemy::defeated() const noexcept {
    return health_ <= epsilon;
}

double Enemy::defeated_at() const noexcept {
    return defeated_at_;
}

TargetState& Enemy::elemental_state() noexcept {
    return elemental_state_;
}

const TargetState& Enemy::elemental_state() const noexcept {
    return elemental_state_;
}

double Enemy::take_damage(double damage, double time_seconds) noexcept {
    if (damage <= 0.0 || defeated()) {
        return 0.0;
    }
    const double applied = std::min(health_, damage);
    health_ -= applied;
    if (health_ <= epsilon) {
        health_ = 0.0;
        defeated_at_ = time_seconds;
    }
    return applied;
}

BattleSimulator::BattleSimulator(
    std::vector<Character> party,
    std::vector<Enemy> enemies,
    BattleConfig config)
    : party_(std::move(party)),
      enemies_(std::move(enemies)),
      config_(std::move(config)),
      reaction_engine_(config_.reactions),
      active_character_index_(config_.initial_active_character) {
    if (party_.size() != 4) {
        throw std::invalid_argument("BattleSimulator requires exactly four characters");
    }
    if (enemies_.empty()) {
        throw std::invalid_argument("BattleSimulator requires at least one enemy");
    }
    if (!std::isfinite(config_.duration_seconds) || config_.duration_seconds <= 0.0 ||
        !finite_nonnegative(config_.switch_cooldown_seconds) ||
        !finite_nonnegative(config_.particle_pickup_delay_seconds) ||
        !std::isfinite(config_.maximum_time_step_seconds) ||
        config_.maximum_time_step_seconds <= 0.0 ||
        active_character_index_ >= party_.size()) {
        throw std::invalid_argument("invalid battle configuration");
    }

    std::unordered_set<std::string> character_ids;
    for (const auto& character : party_) {
        if (!character_ids.insert(character.definition().stats.id).second) {
            throw std::invalid_argument("character ids must be unique");
        }
    }
    std::unordered_set<std::string> enemy_ids;
    for (const auto& enemy : enemies_) {
        if (!enemy_ids.insert(enemy.definition().id).second) {
            throw std::invalid_argument("enemy ids must be unique");
        }
    }
}

BattleResult BattleSimulator::run() {
    if (has_run_) {
        throw std::logic_error("BattleSimulator::run can only be called once");
    }
    has_run_ = true;

    BattleResult result;
    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, ScheduledLater> scheduled;
    std::uint64_t next_sequence = 0;
    double now = 0.0;
    double action_ready_at = 0.0;
    double switch_ready_at = 0.0;
    std::size_t round_robin_cursor = active_character_index_;

    auto add_log = [&](double time,
                       BattleLogKind kind,
                       std::string actor,
                       std::string target,
                       std::string detail,
                       Reaction reaction,
                       double amount) {
        result.log.push_back(
            {time,
             kind,
             std::move(actor),
             std::move(target),
             std::move(detail),
             reaction,
             amount});
    };

    auto all_enemies_defeated = [&]() {
        return std::all_of(enemies_.begin(), enemies_.end(), [](const Enemy& enemy) {
            return enemy.defeated();
        });
    };

    auto record_damage = [&](std::size_t enemy_index,
                             double time,
                             BattleLogKind kind,
                             const std::string& actor,
                             const std::string& detail,
                             Reaction reaction,
                             double raw_damage) {
        auto& enemy = enemies_[enemy_index];
        const bool was_alive = !enemy.defeated();
        const double applied = enemy.take_damage(raw_damage, time);
        result.total_damage += applied;
        add_log(
            time,
            kind,
            actor,
            enemy.definition().id,
            detail,
            reaction,
            applied);
        if (was_alive && enemy.defeated()) {
            add_log(
                time,
                BattleLogKind::enemy_defeated,
                actor,
                enemy.definition().id,
                "enemy health reached zero",
                Reaction::none,
                0.0);
        }
    };

    auto record_reaction = [&](std::size_t enemy_index, const ReactionEvent& event) {
        std::string detail(to_string(event.reaction));
        if (!event.note.empty()) {
            detail += ": ";
            detail += event.note;
        }
        if (event.damage_suppressed) {
            detail += " [damage suppressed]";
        }
        record_damage(
            enemy_index,
            event.time_seconds,
            BattleLogKind::reaction,
            event.source_id,
            detail,
            event.reaction,
            event.independent_damage);
    };

    auto advance_world_to = [&](double time) {
        for (std::size_t i = 0; i < enemies_.size(); ++i) {
            auto& enemy = enemies_[i];
            const double delta = time - enemy.elemental_state().time_seconds();
            if (delta <= epsilon) {
                continue;
            }
            const auto events = reaction_engine_.advance(enemy.elemental_state(), delta);
            for (const auto& event : events) {
                if (enemy.defeated()) {
                    break;
                }
                record_reaction(i, event);
            }
        }
    };

    auto schedule_particle = [&](double time,
                                 std::size_t actor_index,
                                 Element element,
                                 double count,
                                 const std::string& ability_name) {
        ScheduledEvent event;
        event.time_seconds = time;
        event.sequence = next_sequence++;
        event.kind = ScheduledKind::particle;
        event.actor_index = actor_index;
        event.ability_name = ability_name;
        event.particle_element = element;
        event.particle_count = count;
        scheduled.push(std::move(event));
    };

    auto process_particle = [&](const ScheduledEvent& event) {
        for (std::size_t i = 0; i < party_.size(); ++i) {
            auto& character = party_[i];
            double base_energy = 2.0;
            if (event.particle_element != Element::physical) {
                base_energy = character.definition().element == event.particle_element ? 3.0 : 1.0;
            }
            const double field_multiplier = i == active_character_index_ ? 1.0 : 0.6;
            const double requested = event.particle_count * base_energy * field_multiplier *
                                     character.definition().energy_recharge;
            const double before = character.energy_;
            character.set_energy(before + requested);
            const double gained = character.energy_ - before;
            if (gained > epsilon) {
                add_log(
                    event.time_seconds,
                    BattleLogKind::energy_gain,
                    character.definition().stats.id,
                    {},
                    event.ability_name,
                    Reaction::none,
                    gained);
            }
        }
    };

    auto process_hit = [&](const ScheduledEvent& event) {
        auto& character = party_[event.actor_index];
        std::vector<std::size_t> targets;
        if (event.hit.targeting == TargetingMode::primary) {
            for (std::size_t i = 0; i < enemies_.size(); ++i) {
                if (!enemies_[i].defeated()) {
                    targets.push_back(i);
                    break;
                }
            }
        } else {
            for (std::size_t i = 0; i < enemies_.size(); ++i) {
                if (!enemies_[i].defeated()) {
                    targets.push_back(i);
                }
            }
        }

        const bool hit_any_enemy = !targets.empty();
        for (const std::size_t enemy_index : targets) {
            auto& enemy = enemies_[enemy_index];
            Attack attack;
            attack.element = event.hit.element;
            attack.gauge_units = event.hit.gauge_units;
            attack.source = character.definition().stats;
            attack.blunt = event.hit.blunt;
            attack.base_damage = event.hit.base_damage;
            attack.damage_bonus = event.hit.damage_bonus;
            attack.critical_multiplier = event.hit.critical_multiplier;
            attack.defense_ignore = event.hit.defense_ignore;
            if (event.hit.uses_standard_icd) {
                const std::string group = event.hit.icd_group.empty()
                                              ? event.ability_name
                                              : event.hit.icd_group;
                auto& tracker = character.icd_by_target_[enemy.definition().id];
                attack.applies_aura = tracker.can_apply(group, event.time_seconds);
            }

            const Resolution resolution =
                reaction_engine_.apply(enemy.elemental_state(), attack);
            record_damage(
                enemy_index,
                event.time_seconds,
                BattleLogKind::attack_damage,
                character.definition().stats.id,
                event.ability_name,
                Reaction::none,
                resolution.attack_damage);
            for (const auto& reaction : resolution.events) {
                record_reaction(enemy_index, reaction);
            }

            if (event.hit.dendro_core_trigger_limit > 0) {
                const auto core_events = reaction_engine_.trigger_dendro_cores(
                    enemy.elemental_state(),
                    event.hit.element,
                    character.definition().stats,
                    event.hit.dendro_core_trigger_limit);
                for (const auto& reaction : core_events) {
                    record_reaction(enemy_index, reaction);
                }
            }
        }

        if (hit_any_enemy && event.hit.particle_count > epsilon) {
            schedule_particle(
                event.time_seconds + config_.particle_pickup_delay_seconds,
                event.actor_index,
                event.hit.particle_element,
                event.hit.particle_count,
                event.ability_name);
        }
    };

    auto process_due_events = [&]() {
        while (!scheduled.empty() && scheduled.top().time_seconds <= now + epsilon) {
            ScheduledEvent event = scheduled.top();
            scheduled.pop();
            if (event.kind == ScheduledKind::hit) {
                process_hit(event);
            } else {
                process_particle(event);
            }
        }
    };

    auto ability_ready = [&](std::size_t index, AbilityKind kind) {
        return kind == AbilityKind::burst ? party_[index].burst_ready(now)
                                          : party_[index].skill_ready(now);
    };

    auto candidate_for_character = [&](std::size_t index)
        -> std::optional<ActionCandidate> {
        const AbilityKind first =
            config_.burst_before_skill ? AbilityKind::burst : AbilityKind::skill;
        const AbilityKind second =
            config_.burst_before_skill ? AbilityKind::skill : AbilityKind::burst;
        if (ability_ready(index, first)) {
            return ActionCandidate{index, first};
        }
        if (ability_ready(index, second)) {
            return ActionCandidate{index, second};
        }
        return std::nullopt;
    };

    auto choose_action = [&]() -> std::optional<ActionCandidate> {
        if (now + epsilon < switch_ready_at) {
            return candidate_for_character(active_character_index_);
        }

        const AbilityKind first =
            config_.burst_before_skill ? AbilityKind::burst : AbilityKind::skill;
        const AbilityKind second =
            config_.burst_before_skill ? AbilityKind::skill : AbilityKind::burst;
        for (const AbilityKind kind : {first, second}) {
            for (std::size_t offset = 0; offset < party_.size(); ++offset) {
                const std::size_t index = (round_robin_cursor + offset) % party_.size();
                if (ability_ready(index, kind)) {
                    return ActionCandidate{index, kind};
                }
            }
        }
        return std::nullopt;
    };

    auto cast = [&](const ActionCandidate& candidate) {
        auto& character = party_[candidate.actor_index];
        if (candidate.actor_index != active_character_index_) {
            active_character_index_ = candidate.actor_index;
            switch_ready_at = now + config_.switch_cooldown_seconds;
            add_log(
                now,
                BattleLogKind::character_switch,
                character.definition().stats.id,
                {},
                "became the active character",
                Reaction::none,
                0.0);
        }

        const AbilityDefinition& ability = candidate.kind == AbilityKind::skill
                                                ? character.definition().skill
                                                : character.definition().burst;
        if (candidate.kind == AbilityKind::skill) {
            character.skill_ready_at_ = now + ability.cooldown_seconds;
            ++character.skill_cast_count_;
        } else {
            character.energy_ = std::max(0.0, character.energy_ - ability.energy_cost);
            character.burst_ready_at_ = now + ability.cooldown_seconds;
            ++character.burst_cast_count_;
        }

        add_log(
            now,
            BattleLogKind::ability_cast,
            character.definition().stats.id,
            {},
            ability.name,
            Reaction::none,
            0.0);

        for (const auto& hit : ability.hits) {
            ScheduledEvent event;
            event.time_seconds = now + hit.delay_seconds;
            event.sequence = next_sequence++;
            event.kind = ScheduledKind::hit;
            event.actor_index = candidate.actor_index;
            event.ability_name = ability.name;
            event.hit = hit;
            scheduled.push(std::move(event));
        }

        action_ready_at = now + ability.action_duration_seconds;
        round_robin_cursor = (candidate.actor_index + 1) % party_.size();
    };

    auto consider_future_actions = [&](double& next_time) {
        if (action_ready_at > now + epsilon) {
            next_time = std::min(next_time, action_ready_at);
            return;
        }
        if (switch_ready_at > now + epsilon) {
            next_time = std::min(next_time, switch_ready_at);
        }
        for (const auto& character : party_) {
            if (character.skill_ready_at_ > now + epsilon) {
                next_time = std::min(next_time, character.skill_ready_at_);
            }
            if (character.energy_ + epsilon >= character.maximum_energy() &&
                character.burst_ready_at_ > now + epsilon) {
                next_time = std::min(next_time, character.burst_ready_at_);
            }
        }
    };

    while (now < config_.duration_seconds - epsilon) {
        process_due_events();

        if (config_.stop_when_all_enemies_defeated && all_enemies_defeated()) {
            break;
        }

        if (action_ready_at <= now + epsilon) {
            const auto candidate = choose_action();
            if (candidate.has_value()) {
                cast(*candidate);
                continue;
            }
        }

        double next_time = std::min(
            config_.duration_seconds,
            now + config_.maximum_time_step_seconds);
        if (!scheduled.empty()) {
            next_time = std::min(next_time, scheduled.top().time_seconds);
        }
        consider_future_actions(next_time);
        if (next_time <= now + epsilon) {
            next_time = std::min(
                config_.duration_seconds,
                now + config_.maximum_time_step_seconds);
        }

        advance_world_to(next_time);
        now = next_time;
    }

    process_due_events();
    if (now < config_.duration_seconds - epsilon &&
        !(config_.stop_when_all_enemies_defeated && all_enemies_defeated())) {
        advance_world_to(config_.duration_seconds);
        now = config_.duration_seconds;
        process_due_events();
    }

    result.elapsed_seconds = now;
    result.enemies_defeated = static_cast<std::size_t>(std::count_if(
        enemies_.begin(), enemies_.end(), [](const Enemy& enemy) { return enemy.defeated(); }));
    std::stable_sort(
        result.log.begin(), result.log.end(), [](const BattleLogEntry& lhs, const BattleLogEntry& rhs) {
            return lhs.time_seconds < rhs.time_seconds;
        });
    return result;
}

const std::vector<Character>& BattleSimulator::party() const noexcept {
    return party_;
}

const std::vector<Enemy>& BattleSimulator::enemies() const noexcept {
    return enemies_;
}

std::size_t BattleSimulator::active_character_index() const noexcept {
    return active_character_index_;
}

} // namespace elemental
