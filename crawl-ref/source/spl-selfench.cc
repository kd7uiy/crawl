/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"

#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "coordit.h" // radius_iterator
#include "env.h"
#include "god-passive.h"
#include "hints.h"
#include "items.h" // stack_iterator
#include "libutil.h"
#include "message.h"
#include "output.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "spl-other.h"
#include "spl-util.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "tilepick.h"
#include "view.h"

spret cast_deaths_door(int pow, bool fail)
{
    fail_check();
    mpr("You stand defiantly in death's doorway!");
    mprf(MSGCH_SOUND, "You seem to hear sand running through an hourglass...");

    you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                       + (random2(pow) / 10));

    const int hp = max(pow / 10, 1);
    you.attribute[ATTR_DEATHS_DOOR_HP] = hp;
    set_hp(hp);

    if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
        you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    return spret::success;
}

void remove_ice_armour()
{
    mprf(MSGCH_DURATION, "Your icy armour melts away.");
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

spret ice_armour(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("Your icy armour thickens.");
    else if (you.form == transformation::ice_beast)
        mpr("Your icy body feels more resilient.");
    else
        mpr("A film of ice covers your body!");

    you.increase_duration(DUR_ICY_ARMOUR, random_range(40, 50), 50);
    you.props[ICY_ARMOUR_KEY] = pow;
    you.redraw_armour_class = true;

    return spret::success;
}

void fiery_armour()
{
    if (you.duration[DUR_FIERY_ARMOUR])
        mpr("Your cloak of flame flares fiercely!");
    else if (you.duration[DUR_ICY_ARMOUR]
             || you.form == transformation::ice_beast
             || player_icemail_armour_class())
    {
        mprf("A sizzling cloak of flame settles atop your ic%s.",
             you.form == transformation::ice_beast ? "e" : "y armour");
        // TODO: add corresponding inverse message for casting ozo's etc
        // while DUR_FIERY_ARMOUR is active (maybe..?)
    }
    else
        mpr("A protective cloak of flame settles atop you.");

    you.increase_duration(DUR_FIERY_ARMOUR, random_range(110, 140), 1500);
    you.redraw_armour_class = true;
}

spret cast_revivification(int pow, bool fail)
{
    fail_check();
    mpr("Your body is healed in an amazingly painful way.");

    const int loss = 6 + binomial(9, 8, pow);
    dec_max_hp(loss * you.hp_max / 100);
    set_hp(you.hp_max);

    if (you.duration[DUR_DEATHS_DOOR])
    {
        mprf(MSGCH_DURATION, "Your life is in your own hands once again.");
        // XXX: better cause name?
        paralyse_player("Death's Door abortion");
        you.duration[DUR_DEATHS_DOOR] = 0;
    }
    return spret::success;
}

spret cast_swiftness(int power, bool fail)
{
    fail_check();

    if (you.in_liquid())
    {
        // Hint that the player won't be faster until they leave the liquid.
        mprf("The %s foams!", you.in_water() ? "water"
                                             : "liquid ground");
    }

    you.set_duration(DUR_SWIFTNESS, 12 + random2(power)/2, 30,
                     "You feel quick.");
    you.attribute[ATTR_SWIFTNESS] = you.duration[DUR_SWIFTNESS];

    return spret::success;
}

int cast_selective_amnesia(const string &pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return 0;
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    keyin = list_spells(false, false, false, "Forget which spell?");
    redraw_screen();
    update_screen();

    if (isaalpha(keyin))
    {
        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        const bool in_library = you.spell_library[spell];
        if (spell != SPELL_NO_SPELL)
        {
            const string prompt = make_stringf(
                    "Forget %s, freeing %d spell level%s for a total of %d?%s",
                    spell_title(spell), spell_levels_required(spell),
                    spell_levels_required(spell) != 1 ? "s" : "",
                    player_spell_levels(false) + spell_levels_required(spell),
                    in_library ? "" : " This spell is not in your library!");

            if (yesno(prompt.c_str(), in_library, 'n', false))
            {
                if (!pre_msg.empty())
                    mpr(pre_msg);
                del_spell_from_memory_by_slot(slot);
                return 1;
            }
        }
    }

    return -1;
}

spret cast_wereblood(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_WEREBLOOD])
        mpr("Your blood is freshly infused with primal strength!");
    else
        mpr("Your blood is infused with primal strength.");

    you.set_duration(DUR_WEREBLOOD, 20 + random2avg(pow, 2));

    you.props[WEREBLOOD_KEY] = 0;
    return spret::success;
}

int silence_min_range(int pow)
{
    return shrinking_aoe_range((20 + pow/5) * BASELINE_DELAY);
}

int silence_max_range(int pow)
{
    return shrinking_aoe_range((19 + pow/5 + pow/2) * BASELINE_DELAY);
}

spret cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("A profound silence engulfs you.");

    you.increase_duration(DUR_SILENCE, 20 + div_rand_round(pow,5)
                            + random2avg(div_rand_round(pow,2), 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return spret::success;
}

int liquefaction_max_range(int pow)
{
    return shrinking_aoe_range((9 + pow) * BASELINE_DELAY);
}

spret cast_liquefaction(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, BROWN, 80);
    flash_view_delay(UA_PLAYER, YELLOW, 80);
    flash_view_delay(UA_PLAYER, BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return spret::success;
}

spret cast_transform(int pow, transformation which_trans, bool fail)
{
    if (!transform(pow, which_trans, false, true)
        || !check_form_stat_safety(which_trans))
    {
        return spret::abort;
    }

    fail_check();
    transform(pow, which_trans);
    return spret::success;
}
