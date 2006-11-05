#include <config.h>
#include <kernel/eressea.h>
#include "items.h"

#include "study.h"

#include <kernel/curse.h>
#include <kernel/building.h>
#include <kernel/faction.h>
#include <kernel/item.h>
#include <kernel/magic.h>
#include <kernel/message.h>
#include <kernel/movement.h>
#include <kernel/order.h>
#include <kernel/plane.h>
#include <kernel/pool.h>
#include <kernel/region.h>
#include <kernel/ship.h>
#include <kernel/skill.h>
#include <kernel/spell.h>
#include <kernel/unit.h>

#include <items/demonseye.h>

#include <util/attrib.h>
#include <util/functions.h>
#include <util/rand.h>

#include <limits.h>

/* BEGIN studypotion */
#define MAXGAIN 15
static int
use_studypotion(struct unit * u, const struct item_type * itype, int amount, struct order * ord)
{
  if (get_keyword(u->thisorder) == K_STUDY) {
    skill_t sk;
    skill * sv;

    init_tokens(u->thisorder);
    skip_token();
    sk = findskill(getstrtoken(), u->faction->locale);
    sv = get_skill(u, sk);

    if (sv && sv->level > 2) {
      /* TODO: message */
    } else if (study_cost(u, sk)>0) {
      /* TODO: message */
    } else {
      attrib * a = a_find(u->attribs, &at_learning);
      teaching_info * teach;
      if (a==NULL) {
        a = a_add(&u->attribs, a_new(&at_learning));
      }
      teach = (teaching_info*) a->data.v;
      if (amount>MAXGAIN) amount = MAXGAIN;
      teach->value += amount * 30;
      if (teach->value > MAXGAIN * 30) {
        teach->value = MAXGAIN * 30;
      }
      i_change(&u->items, itype, -amount);
      return 0;
    }
  }
  return EUNUSABLE;
}
/* END studypotion */

/* BEGIN speedsail */
#define SPEEDSAIL_EFFECT 1
static int
use_speedsail(struct unit * u, const struct item_type * itype, int amount, struct order * ord)
{
  curse  *c;
  variant effect;
  ship * sh = u->ship;
  if (!sh) {
    cmistake(u, ord, 20, MSG_MOVE);
    return -1;
  }

  effect.i = SPEEDSAIL_EFFECT;
  c = create_curse(u, &sh->attribs, ct_find("shipspeedup"), 20, INT_MAX, effect, 0);
  curse_setflag(c, CURSE_NOAGE);

  ADDMSG(&u->faction->msgs, msg_message("use_speedsail", "unit speed", u, SPEEDSAIL_EFFECT));
  itype->rtype->uchange(u, itype->rtype, -1);

  return 0;
}
/* END speedsail */

/* ------------------------------------------------------------- */
/* Kann auch von Nichtmagiern benutzt werden, erzeugt eine
* Antimagiezone, die zwei Runden bestehen bleibt */
static int
use_antimagiccrystal(unit * u, const struct item_type * itype, int amount, struct order * ord)
{
  region * r = u->region;
  const resource_type * rt_crystal = NULL;
  int i;

  if (rt_crystal == NULL) {
    rt_crystal = rt_find("antimagic");
    assert(rt_crystal!=NULL);
  }
  for (i=0;i!=amount;++i) {
    int effect, duration = 2;
    double force;
    spell *sp = find_spell(M_GRAU, "antimagiczone");
    attrib ** ap = &r->attribs;
    unused(ord);
    assert(sp);

    /* Reduziert die St�rke jedes Spruchs um effect */
    effect = sp->level; 

    /* H�lt Spr�che bis zu einem summierten Gesamtlevel von power aus.
    * Jeder Zauber reduziert die 'Lebenskraft' (vigour) der Antimagiezone
    * um seine Stufe */
    force = sp->level * 20; /* Stufe 5 =~ 100 */

    /* Regionszauber aufl�sen */
    while (*ap && force > 0) {
      curse * c;
      attrib * a = *ap;
      if (!fval(a->type, ATF_CURSE)) {
        do { ap = &(*ap)->next; } while (*ap && a->type==(*ap)->type);
        continue;
      }
      c = (curse*)a->data.v;

      /* Immunit�t pr�fen */
      if (c->flag & CURSE_IMMUNE) {
        do { ap = &(*ap)->next; } while (*ap && a->type==(*ap)->type);
        continue;
      }

      force = destr_curse(c, effect, force);
      if(c->vigour <= 0) {
        a_remove(&r->attribs, a);
      }
      if(*ap) ap = &(*ap)->next;
    }

    if(force > 0) {
      variant var ;
      var.i = effect;
      create_curse(u, &r->attribs, ct_find("antimagiczone"), force, duration, var, 0);
    }
  }
  use_pooled(u, rt_crystal, GET_SLACK|GET_RESERVE|GET_POOLED_SLACK, amount);
  ADDMSG(&u->faction->msgs, msg_message("use_antimagiccrystal", 
    "unit region", u, r));
  return 0;
}

static int
use_instantartsculpture(struct unit * u, const struct item_type * itype,
                        int amount, struct order * ord)
{
  building *b;

  if(u->region->land == NULL) {
    cmistake(u, ord, 242, MSG_MAGIC);
    return -1;
  }

  b = new_building(bt_find("artsculpture"), u->region, u->faction->locale);
  b->size = 100;
  sprintf(buf, "%s", LOC(u->faction->locale, "artsculpture"));
  set_string(&b->name, buf);

  ADDMSG(&u->region->msgs, msg_message("artsculpture_create", "unit region", 
    u, u->region));

  itype->rtype->uchange(u, itype->rtype, -1);

  return 0;
}

static int
use_instantartacademy(struct unit * u, const struct item_type * itype,
                      int amount, struct order * ord)
{
  building *b;

  if(u->region->land == NULL) {
    cmistake(u, ord, 242, MSG_MAGIC);
    return -1;
  }

  b = new_building(bt_find("artacademy"), u->region, u->faction->locale);
  b->size = 100;
  sprintf(buf, "%s", LOC(u->faction->locale, "artacademy"));
  set_string(&b->name, buf);

  ADDMSG(&u->region->msgs, msg_message(
    "artacademy_create", "unit region", u, u->region));

  itype->rtype->uchange(u, itype->rtype, -1);

  return 0;
}

#define BAGPIPEFRACTION dice_rand("2d4+2")
#define BAGPIPEDURATION dice_rand("2d10+4")

static int
use_bagpipeoffear(struct unit * u, const struct item_type * itype,
                  int amount, struct order * ord)
{
  int money;
  variant effect;

  if(get_curse(u->region->attribs, ct_find("depression"))) {
    cmistake(u, ord, 58, MSG_MAGIC);
    return -1;
  }

  money = entertainmoney(u->region)/BAGPIPEFRACTION;
  change_money(u, money);
  rsetmoney(u->region, rmoney(u->region) - money);

  effect.i = 0;
  create_curse(u, &u->region->attribs, ct_find("depression"),
    20, BAGPIPEDURATION, effect, 0);

  ADDMSG(&u->faction->msgs, msg_message("bagpipeoffear_faction",
    "unit region command money", u, u->region, ord, money));

  ADDMSG(&u->region->msgs, msg_message("bagpipeoffear_region",
    "unit money", u, money));

  return 0;
}

static int
use_aurapotion50(struct unit * u, const struct item_type * itype,
                 int amount, struct order * ord)
{
  if(!is_mage(u)) {
    cmistake(u, ord, 214, MSG_MAGIC);
    return -1;
  }

  change_spellpoints(u, 50);

  ADDMSG(&u->faction->msgs, msg_message("aurapotion50",
    "unit region command", u, u->region, ord));

  itype->rtype->uchange(u, itype->rtype, -1);

  return 0;
}


void
register_itemfunctions(void)
{
  register_demonseye();
  register_function((pf_generic)use_antimagiccrystal, "use_antimagiccrystal");
  register_function((pf_generic)use_instantartsculpture, "use_instantartsculpture");
  register_function((pf_generic)use_studypotion, "use_studypotion");
	register_function((pf_generic)use_speedsail, "use_speedsail");
  register_function((pf_generic)use_instantartacademy, "use_instantartacademy");
  register_function((pf_generic)use_bagpipeoffear, "use_bagpipeoffear");
  register_function((pf_generic)use_aurapotion50, "use_aurapotion50");
}
