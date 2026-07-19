import fs from "node:fs";
import path from "node:path";

const root = process.argv[2] || "assets";
const eventsDir = path.join(root, "data", "events");
const manifest = JSON.parse(fs.readFileSync(path.join(eventsDir, "manifest.json"), "utf8"));

const themes = {
  labor: {
    words: ["worker", "workshop", "smith", "tailor", "janitor", "apprentice", "mine", "loader", "crew", "wage", "work"],
    frames: [
      "The hands doing the work have asked why the hands holding the deed make every decision.",
      "Management has described exhaustion as an exciting form of employee ownership.",
      "The people maintaining this place know exactly how it fails and are forbidden to say so near customers.",
      "A supervisor has arrived to explain the task to everyone who has performed it for twenty years.",
      "The work is necessary, the workers are disposable, and the contradiction has begun making noise.",
      "Someone has posted a safety notice where only the injured can read it.",
      "The labor is collective, although the reward has wandered off alone.",
      "The shift bell rings with the confidence of an object that has never worked a shift."
    ]
  },
  housing: {
    words: ["rent", "house", "home", "property", "estate", "room", "inn", "hut", "shelter", "evict", "landlord", "tenant"],
    frames: [
      "Housing has become scarce since a distant heir discovered that doors can collect income.",
      "The landlord calls this shelter; the roof calls it a temporary misunderstanding.",
      "Three families need this space, while one absent owner needs a fourth summer palace.",
      "A rent notice has been nailed to something that is not legally a building but is legally afraid.",
      "The property office recognizes ownership more readily than occupancy, repair, or breathing.",
      "Someone bought the neighborhood as an investment and has not yet learned it contains people.",
      "Every available room is affordable to someone who does not need it.",
      "The deed is immaculate. The walls, tenants, and moral argument are damp."
    ]
  },
  antifascist: {
    words: ["league", "uniform", "tyrant", "brown cape", "suprem", "purity", "hierarchy", "patrol", "authoritarian", "fasc"],
    frames: [
      "Respectable merchants have funded men in matching clothes to make hierarchy look spontaneous.",
      "A frightened little movement has found uniforms, sponsors, and several louder frightened men.",
      "The local strongmen demand obedience while arguing over whose boots look most traditional.",
      "Power is rehearsing violence in public and calling the rehearsal civic pride.",
      "The people preaching order have arrived first to create the disorder they require.",
      "A hierarchy club insists it is merely a walking group with enemies.",
      "The uniforms are new, the grievance is manufactured, and the wealthy patrons are suddenly camera shy.",
      "Someone must interrupt this pageant before the pageant receives a police budget."
    ]
  },
  antiwar: {
    words: ["war", "army", "soldier", "general", "battle", "siege", "weapon", "cannon", "deserter", "recruit", "military"],
    frames: [
      "The generals call this a front; the families living here continue calling it home.",
      "A mapmaker has moved the border again without consulting the houses underneath the ink.",
      "The war needs one more sacrifice and has selected someone who did not attend the meeting.",
      "Recruiters promise glory in quantities inversely proportional to their distance from danger.",
      "Two rulers dispute a hill and have sent several thousand unrelated people to clarify ownership.",
      "The uniforms disagree about flags and agree completely about who carries the wounded.",
      "Peace is available, but several important careers depend on misplacing it.",
      "The army has requisitioned common sense for ceremonial use."
    ]
  },
  anti_genocide: {
    words: ["refugee", "displaced", "border", "purge", "massacre", "grave", "memorial", "survivor", "exile", "return", "testimony"],
    frames: [
      "The survivors carry names that an efficient office tried to turn into numbers.",
      "The border is closed to the people whose homes were opened by artillery.",
      "Officials debate terminology while families count who did not arrive.",
      "Someone preserved the testimony because power was already preparing to forget.",
      "The killers called it administration; the living have brought names, dates, and witnesses.",
      "A return route survives in memory even though every official map denies it.",
      "The refugees have been asked to prove they fled an event the authorities claim never happened.",
      "The archive is incomplete because survival took priority over filing."
    ]
  },
  ecology: {
    words: ["forest", "swamp", "river", "water", "tree", "animal", "beast", "pollut", "sludge", "storm", "tide", "frog", "bee", "mushroom", "climate"],
    frames: [
      "The poison travels downstream for free while responsibility requires a toll permit.",
      "A company has discovered that nature is priceless and therefore available at no cost.",
      "The local wildlife has filed its complaint through teeth, weather, and one unusually organized fungus.",
      "Profit left the region yesterday. The smoke, invoices, and cough remain.",
      "The forest has no legal standing but considerably more roots than the courthouse.",
      "An extraction guild calls the damage temporary because mountains use a longer calendar.",
      "The water is changing color in a way the refinery describes as innovative.",
      "Everyone agrees the land must be saved immediately after the next profitable quarter."
    ]
  },
  care: {
    words: ["clinic", "medicine", "heal", "injured", "sick", "child", "mother", "funeral", "rescue", "patient", "grief", "hospital"],
    frames: [
      "Care is being provided by people too tired to qualify for the care they provide.",
      "The emergency is obvious, but compassion has not completed the required procurement form.",
      "Someone needs help now, which is earlier than the institution's next available appointment.",
      "The healers have run out of supplies and begun prescribing solidarity in dangerous doses.",
      "Grief has arrived without identification and the clerk is unsure where to seat it.",
      "The person doing the caring is treated as furniture until the furniture asks for sleep.",
      "A life can be saved cheaply, creating a serious problem for the people charging more.",
      "The waiting room has formed a government because the official one is late."
    ]
  },
  food: {
    words: ["bread", "food", "meal", "stew", "kitchen", "cook", "tavern", "beer", "wine", "berry", "cheese", "hunger", "harvest"],
    frames: [
      "There is enough food nearby, although ownership has placed itself between appetite and lunch.",
      "The kitchen can feed everyone or satisfy the ledger, but apparently not both before supper.",
      "Hunger has been classified as a personal budgeting error by people eating dessert.",
      "A full pantry and an empty street are conducting a quiet argument about distribution.",
      "The cooks have begun measuring wealth in portions rather than coins, alarming the merchants.",
      "The harvest was excellent until three contracts and a duke ate it.",
      "Someone has locked up tomorrow's bread to improve today's price.",
      "The soup is communal, the spoons are disputed, and nobody is leaving hungry without a debate."
    ]
  },
  colonialism: {
    words: ["ancient", "ruin", "artifact", "relic", "claim", "native", "expedition", "museum", "map", "discovery", "settler", "buried"],
    frames: [
      "The expedition has discovered a place whose residents made the tactical error of already living here.",
      "A museum label calls this object found; the family searching for it uses a different verb.",
      "The newest map begins with blank land and ends with somebody else's farms.",
      "An empire left ruins, debts, and a heroic plaque describing only the ruins.",
      "The claim is legally ancient and the resistance is merely older.",
      "A collector calls this preservation because theft sounds unprofessional on a grant request.",
      "The border follows a ruler's old argument and cuts directly through a community's kitchen.",
      "History was written by the conqueror, edited by the insurer, and corrected in charcoal by everyone else."
    ]
  },
  capital: {
    words: ["merchant", "money", "gold", "tax", "auction", "vendor", "company", "credit", "bank", "price", "profit", "insurance", "debt", "equity", "contract"],
    frames: [
      "An investor has purchased the problem, removed everything useful, and listed the remainder as growth.",
      "The price rose after someone important bought all the alternatives.",
      "A merchant has found a way to own the risk while renting it back to everyone in danger.",
      "The ledger reports excellent health and requests that nobody examine the patient.",
      "Profit has been declared the only adult in the room despite eating all the furniture.",
      "A debt collector has arrived to repossess something the debtor grew personally.",
      "The market is invisible except when it needs guards, roads, courts, or lunch.",
      "A financier calls the missing roof an efficiency and the rain a subscription service."
    ]
  },
  post_scarcity: {
    words: ["spaceship", "crash", "star", "sky", "hologram", "robot", "fabricator", "automated", "future", "credit chip", "black box"],
    frames: [
      "The machine can provide abundance, but its license agreement remains emotionally committed to scarcity.",
      "A visitor from a better future has encountered an ancient and apparently immortal invoice.",
      "The technology eliminated want before a committee restored it for market stability.",
      "This device can rearrange matter but cannot override a regional manager.",
      "The stars contain wonders, several of which require a proof of purchase.",
      "Automation has freed everyone from labor except the people repairing the automation.",
      "A post-scarcity tool has been fitted with a coin slot for cultural compatibility.",
      "Humanity reached the heavens and discovered procurement had arrived earlier."
    ]
  },
  liberalism: {
    words: ["council", "committee", "permit", "hearing", "petition", "moderation", "compromise", "procedure", "meeting", "clerk", "bureaucr", "paperwork"],
    frames: [
      "The council agrees something must be done after a respectful period of doing nothing.",
      "A committee has bravely condemned the temperature of the fire while declining to inconvenience the arsonist.",
      "Procedure can save lives today, provided nobody powerful objects before closing time.",
      "The moderates have proposed meeting the boot halfway and are surprised by its direction of travel.",
      "Everyone supports justice in principle; the agenda has allocated principle seven minutes.",
      "The institution is capable of courage but requires three signatures from people selected for caution.",
      "A listening session is underway while the people being discussed shout through the window.",
      "The resolution contains strong language, weak verbs, and a catered lunch."
    ]
  },
  state_power: {
    words: ["guard", "prison", "execution", "law", "fine", "wanted", "judge", "court", "jail", "police", "magistrate", "license"],
    frames: [
      "The law has arrived carrying a weapon and an incomplete understanding of the law.",
      "Authority has mistaken being obeyed for being correct, a common occupational injury.",
      "The prison calls its cages rehabilitation and has misplaced the second half of the process.",
      "A guard is enforcing an ordinance invented moments ago by the guard.",
      "The state can locate a hungry thief faster than a missing wage.",
      "The court recognizes property, rank, and lunchtime, roughly in that order.",
      "Surveillance has made everyone safer from privacy and little else.",
      "The ministry owns the building; the old boss still owns every decision inside it."
    ]
  },
  union: {
    words: ["union", "strike", "guild", "picket", "charter", "collective", "shop floor", "delegate", "dues"],
    frames: [
      "The workers agree the boss is a problem and have opened a longer argument about everything after that.",
      "The picket line has survived rain, threats, and a three-hour dispute over the soup rota.",
      "A union meeting is converting private fear into public disagreement, which is progress.",
      "Management calls the strike disorder because the usual order was profitable.",
      "The charter promises democracy and now must endure the terrible inconvenience of members.",
      "A paid negotiator has arrived to explain patience to people missing three paychecks.",
      "The scabs were lied to, the strikers are furious, and the owner is vacationing beyond thrown-vegetable range.",
      "Collective power has formed a committee and immediately discovered collective paperwork."
    ]
  },
  revolution: {
    words: ["revolution", "uprising", "rebel", "barricade", "throne", "crown", "king", "overthrow", "rebellion", "liberation"],
    frames: [
      "The last revolution failed, leaving behind songs, scars, and several people taking better notes.",
      "Everyone wants the tyrant gone; nobody agrees who washes the cups on the morning after.",
      "Hope has returned wearing patched boots and carrying minutes from the previous defeat.",
      "The barricade is impractical, badly painted, and the first honest architecture in the district.",
      "A movement can lose the square and still teach a generation where the square is.",
      "The old order looks permanent because it employs historians.",
      "Victory is possible, failure is likely, and surrender remains the least interesting option.",
      "The rebels have a plan, a counterplan, and one kettle recognized by every faction."
    ]
  },
  freedom: {
    words: ["pirate", "ship", "sea", "captain", "cage", "prisoner", "escape", "scooter", "road", "smuggler", "sail"],
    frames: [
      "Freedom begins where someone can refuse an order without losing dinner.",
      "The crew elects its captain and reserves the right to unelect them before dessert.",
      "A border guard has confused movement with permission and permission with virtue.",
      "The pirates recognize fewer property laws and considerably more lunch breaks.",
      "An open road offers liberty, weather, and no guarantee of material support.",
      "The cage is legal, the escape is necessary, and the locksmith prefers cash.",
      "Nobody aboard is free while one person owns the boat and everyone else's sleep.",
      "The authorities call this smuggling because rescue would imply a duty."
    ]
  },
  science: {
    words: ["crystal", "observatory", "telescope", "book", "scholar", "experiment", "puzzle", "magic", "recording", "research", "archive", "study"],
    frames: [
      "The evidence is magnificent, inconvenient, and therefore awaiting a more cooperative truth.",
      "A discovery belongs to everyone until the grant office locates a lock.",
      "The researchers know how the danger works and lack permission to stop it.",
      "Knowledge has advanced faster than the committee responsible for approving knowledge.",
      "The experiment was designed to improve life and funded to improve quarterly output.",
      "A scholar has found the answer and is now searching for an institution willing to hear it.",
      "The universe is under no obligation to respect the official diagram.",
      "The instrument is precise, the theory is provisional, and the sponsor wants a slogan."
    ]
  },
  faith: {
    words: ["god", "altar", "shrine", "priest", "saint", "bless", "curse", "demon", "witch", "prophet", "holy", "temple"],
    frames: [
      "The demon has a mother, a lunch break, and a complaint about unsafe summoning circles.",
      "A local grandmother has invited the haunting to tea and learned it takes three sugars.",
      "The temple owns gold cups while its god keeps asking why the neighbors are hungry.",
      "Faith has arrived as practical courage; authority has arrived dressed like faith.",
      "The miracle is genuine, although the licensing office suspects unauthorized hope.",
      "A priest promises heaven later while the congregation organizes breakfast now.",
      "The sacred text is clear, except where clarity would reduce the bishop's income.",
      "The monster would like to be treated as a person before discussing the screaming."
    ]
  },
  propaganda: {
    words: ["news", "poster", "pamphlet", "recording", "ledger", "statue", "portrait", "newspaper", "broadcast"],
    frames: [
      "The official account is polished, confident, and contradicted by every person who was there.",
      "A lie has acquired a printing budget and now expects to be addressed as public information.",
      "The newspaper reports calm beneath a headline printed while the office shook.",
      "Power has commissioned a statue to remember events in the preferred direction.",
      "The rumor is inaccurate in detail and devastatingly correct about ownership.",
      "A ministry poster asks the hungry to defend the pantry from other hungry people.",
      "The archive contains two histories: the bound edition and the notes hidden behind it.",
      "Everyone witnessed the event, which is why the authorities require an approved version."
    ]
  },
  kindness: {
    words: ["help", "friend", "companion", "stranger", "gift", "mercy", "apology", "lost", "rescue", "share", "kind"],
    frames: [
      "Someone has made the strategically indefensible decision to treat a stranger as a person.",
      "Kindness has no budget, no armor, and an alarming record of surviving anyway.",
      "The practical choice is cruelty; the useful choice may be refusing that definition of practical.",
      "A small mercy cannot repair the system, but it can keep one person alive to organize tomorrow.",
      "Nobody expects help, making help both naive and tactically surprising.",
      "The institution has abandoned this person; several ordinary people have not.",
      "Compassion will cost more than indifference and purchase something the ledger cannot name.",
      "The world is terrible at rewarding decency and strangely dependent upon it."
    ]
  }
};

const locationPreferences = {
  city: ["housing", "labor", "capital", "state_power", "liberalism", "care"],
  tavern: ["labor", "union", "food", "kindness", "propaganda"],
  road: ["antiwar", "freedom", "colonialism", "kindness", "propaganda"],
  forest: ["ecology", "colonialism", "food", "faith", "kindness"],
  dungeon: ["state_power", "capital", "revolution", "colonialism", "labor"],
  cave: ["labor", "ecology", "science", "capital", "colonialism"],
  swamp: ["ecology", "care", "colonialism", "kindness", "science"],
  mountains: ["labor", "ecology", "science", "antiwar", "colonialism"],
  coast: ["freedom", "labor", "anti_genocide", "ecology", "capital"],
  sea: ["freedom", "antiwar", "labor", "anti_genocide", "ecology"],
  crash: ["post_scarcity", "science", "capital", "liberalism", "care"],
  dungeon_finale: ["revolution", "state_power", "capital", "kindness", "antiwar"],
  special: ["kindness", "faith", "revolution", "care", "propaganda"]
};

const approachRules = [
  ["solidarity", /organize|union|picket|share|cooperative|collective|stand with|distribute|free the|join them|help the workers|back the crew|support the/],
  ["mercy", /help|heal|comfort|feed|listen|apologi|rescue|save |protect|give |tip generously|show kindness|treat /],
  ["greed", /steal|rob|loot|pocket|sell |buy |take the coin|take the gold|charge |profit|extort|blackmail|keep it|claim it/],
  ["force", /attack|fight|threaten|break |burn |smash|kick|punch|kill|draw your|challenge|intimidate/],
  ["law", /report|petition|permit|obey|pay the|sign |vote|appeal|ask the guard|call the guard|legal|official/],
  ["inquiry", /ask |read|study|investigate|inspect|figure out|decode|listen|talk|debate|examine|search|follow|learn/],
  ["caution", /leave|ignore|walk away|flee|refuse|decline|avoid|wait|do nothing|back away/],
  ["wit", /trick|lie|bluff|joke|perform|distract|pretend|argue|improvise|outsmart/]
];

const politicalEffects = {
  solidarity: "region solidarity +1",
  mercy: "region solidarity +1",
  greed: "region solidarity -1",
  force: "region unrest +1",
  law: "region pressure +1"
};

const exactThemes = {
  cave_wrong_echo: "science",
  cave_sleeping_something: "labor",
  cave_glow_worms: "ecology",
  cave_underground_lake: "ecology",
  cave_crystal_choir: "science",
  city_book_stall: "propaganda",
  city_doom_prophet: "faith",
  city_pickpocket: "capital",
  city_guard_shakedown: "state_power",
  city_spaceship_parts: "post_scarcity",
  city_lost_noble_child: "kindness",
  city_smith_shop: "labor",
  city_general_store: "capital",
  city_faction_recruiter: "antiwar",
  city_fountain_wish: "kindness",
  city_street_magician: "labor",
  city_bathhouse: "care",
  city_execution_crowd: "state_power",
  city_credit_exchange: "capital",
  dungeon_artifact_find: "colonialism",
  dungeon_no_refunds_door: "capital",
  dungeon_ribcage_sword: "antiwar",
  dungeon_goblin_toll: "capital",
  dungeon_forgotten_altar: "faith",
  dungeon_slime_janitorial: "labor",
  dungeon_puzzle_door: "science",
  dungeon_mimic_support_group: "kindness",
  dungeon_lich_taxes: "capital",
  dungeon_finale_vault: "capital",
  dungeon_finale_janitor: "labor",
  dungeon_finale_gift_shop: "capital",
  forest_talking_mushroom: "ecology",
  forest_bureaucratic_bear: "state_power",
  forest_definitely_safe_berries: "food",
  forest_witch_hut: "faith",
  forest_mushroom_circle: "faith",
  forest_scooter_wreck: "freedom",
  forest_beehive: "union",
  road_scooter_gang: "freedom",
  road_broken_wagon: "kindness",
  road_peddler: "capital",
  road_hitchhiking_wizard: "kindness",
  road_automated_toll: "post_scarcity",
  wild_campfire: "kindness",
  road_funeral_procession: "care",
  road_signpost_liar: "propaganda",
  hidden_glade: "ecology",
  tavern_rumor: "propaganda",
  tavern_fizzing_drink: "science",
  tavern_arm_wrestle: "labor",
  tavern_mystery_stew: "food",
  tavern_bard_ballad: "propaganda",
  tavern_tab_dispute: "capital",
  tavern_cartographer: "colonialism",
  tavern_living_legend: "propaganda",
  tavern_descendant: "kindness",
  tavern_dice_game: "capital",
  tavern_karaoke_bard: "kindness",
  tavern_mystery_keg: "food",
  mountain_rope_bridge: "freedom",
  mountain_monastery_silence: "faith",
  r12_rooftop_thief: "capital",
  r12_thief_returns: "kindness",
  r12_thief_finale: "state_power",
  r12_old_acquaintance: "kindness",
  r15_payroll_oracle: "labor",
  r15_rent_as_weather: "housing",
  r15_eviction_morning: "housing",
  r15_refinery_children: "ecology",
  r15_sunlight_union: "ecology",
  r15_uniform_sample: "antifascist",
  r15_banner_march: "antifascist",
  r15_border_train: "anti_genocide",
  r15_deserters_map: "antiwar",
  r15_war_bond_gala: "antiwar",
  r15_committee_against_urgency: "liberalism",
  r15_necromantic_equity: "capital",
  r15_carbon_offset_ogre: "ecology",
  r15_mine_rescue_vote: "labor",
  r15_public_clinic_queue: "care",
  r15_mutual_aid_soup: "food",
  r15_post_scarcity_invoice: "post_scarcity",
  r15_replicator_license: "post_scarcity",
  r15_pirate_labor_code: "freedom",
  r15_harbor_strike: "union",
  r15_state_factory: "state_power",
  r15_police_vendor: "state_power",
  r15_propaganda_correction: "propaganda",
  r15_union_election: "union",
  r15_scab_caravan: "union",
  r15_climate_wall: "ecology",
  r15_rescue_at_sea: "anti_genocide",
  r15_commons_fence: "colonialism",
  r15_failed_revolution_picnic: "revolution",
  r15_kindness_audit: "kindness"
};

const idThemeRules = [
  ["antifascist", /fasc|uniform|league|strongman|suprem|purity|tyrant/],
  ["anti_genocide", /refugee|border|grave|memorial|exile|surviv|return_route|massacre|testimony/],
  ["antiwar", /war|soldier|deserter|army|battle|siege|cannon|weapon|recruiter|quartermaster/],
  ["union", /union|strike|picket|guild_(vote|strike|split|union)|shop_floor|charter/],
  ["labor", /worker|payroll|mine_|smith|tailor|janitor|apprentice|dockworker|shipwright|road_crew|torch_goblin/],
  ["housing", /rent|evict|tenant|estate|housing|landlord|zoning|room|walking_town/],
  ["ecology", /pollut|refinery|climate|forest_|swamp_|glacier|whale|frog|beast|oak|mushroom|bee|tide|storm/],
  ["care", /clinic|medicine|healer|injured|rescue|funeral|wake|patient|plague|grief/],
  ["food", /bread|food|kitchen|stew|cook|harvest|berry|cheese|wine|ale|keg|cask|egg/],
  ["post_scarcity", /crash|spacer|spaceship|hologram|terminal|fabricator|replicator|cryo|black_box/],
  ["faith", /god|altar|shrine|temple|saint|curse|witch|prophet|idol|pilgrim|oracle/],
  ["science", /crystal|observatory|telescope|experiment|puzzle|echo|archive|museum|scholar|recording/],
  ["propaganda", /news|poster|pamphlet|broadsheet|statue|portrait|rumor|ledger|ballad|graffiti/],
  ["state_power", /guard|prison|execution|court|judge|curfew|stocks|patrol|law|permit|license/],
  ["liberalism", /committee|council|hearing|moderation|bureaucr|paperwork/],
  ["revolution", /revolution|uprising|rebel|barricade|throne|crown|overthrow|liberat/],
  ["freedom", /pirate|sloop|sail|ship_|smuggler|scooter|escape|captain|open_water/],
  ["colonialism", /artifact|relic|ruin|ancient|tomb|claim|cartograph|expedition|discovery/],
  ["capital", /merchant|credit|tax|auction|vendor|shop|treasury|toll|bank|debt|equity|contract|bounty|market|refund|vault/],
  ["kindness", /companion|lost|wedding|friend|stranger|gift|mercy|apology|orphan/]
];

function hash32(value) {
  let h = 2166136261;
  for (const ch of value) {
    h ^= ch.charCodeAt(0);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

function eventPrimary(event) {
  const locations = Array.isArray(event.locations) ? event.locations : [];
  return locations.length ? locations[0] : "special";
}

function sourceText(event) {
  const bits = [event.id, event.text];
  for (const choice of event.choices || []) {
    bits.push(choice.text || "");
    for (const key of ["outcomes", "success", "fail"])
      for (const outcome of choice[key] || []) bits.push(outcome.text || "");
  }
  return bits.join(" ").toLowerCase();
}

function hasPhrase(source, phrase) {
  const escaped = phrase.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  return new RegExp(`(^|[^a-z0-9])${escaped}([^a-z0-9]|$)`, "i").test(source);
}

function chooseTheme(event, counts) {
  if (exactThemes[event.id]) return exactThemes[event.id];
  for (const [theme, pattern] of idThemeRules)
    if (pattern.test(event.id) && (counts[theme] || 0) < 50) return theme;
  const openingSource = `${event.id} ${event.text}`.toLowerCase();
  const source = sourceText(event);
  const primary = eventPrimary(event);
  const rawScores = {};
  for (const [name, def] of Object.entries(themes)) {
    let raw = 0;
    for (const word of def.words) {
      if (hasPhrase(openingSource, word)) raw += word.length > 7 ? 110 : 90;
      else if (hasPhrase(source, word)) raw += word.length > 7 ? 24 : 18;
    }
    rawScores[name] = raw;
  }
  let candidates = Object.keys(themes).filter(name => rawScores[name] > 0 && (counts[name] || 0) < 50);
  if (!candidates.length)
    candidates = (locationPreferences[primary] || ["kindness", "care", "science"])
      .filter(name => (counts[name] || 0) < 50);
  if (!candidates.length)
    candidates = Object.keys(themes).filter(name => (counts[name] || 0) < 50);
  let best = candidates[0] || "kindness";
  let bestScore = -100000;
  for (const name of candidates) {
    let score = rawScores[name];
    if ((locationPreferences[primary] || []).includes(name)) score += 14;
    score -= counts[name] || 0;
    score += hash32(event.id + ":" + name) % 7;
    if (score > bestScore) {
      best = name;
      bestScore = score;
    }
  }
  return best;
}

function inferApproach(text, index) {
  const source = String(text || "").toLowerCase();
  for (const [name, pattern] of approachRules)
    if (pattern.test(source)) return name;
  return ["engage", "pragmatic", "defiance", "caution"][index % 4];
}

function caseTitle(id) {
  const words = id.split("_").filter((word, index) =>
    !(index === 0 && /^(r\d+|b2[a-z])$/.test(word)));
  return words.map(word => word.length ? word[0].toUpperCase() + word.slice(1) : word).join(" ");
}

const codaTemplates = [
  (title, claim) => `The file titled "${title}" makes the arrangement plain: ${claim}.`,
  (title, claim) => `Officials classify "${title}" as routine, although ${claim}.`,
  (title, claim) => `Ask who pays for "${title}" and the answer begins here: ${claim}.`,
  (title, claim) => `The paperwork around "${title}" avoids its central fact: ${claim}.`,
  (title, claim) => `Power calls "${title}" an exception. The rule it conceals is simpler: ${claim}.`,
  (title, claim) => `Behind the business of "${title}" sits an older truth: ${claim}.`,
  (title, claim) => `Nobody affected by "${title}" was consulted. The part authority avoids is that ${claim}.`,
  (title, claim) => `The official lesson of "${title}" is complicated; the useful lesson is that ${claim}.`,
  (title, claim) => `People remember "${title}" differently, but the material fact remains: ${claim}.`,
  (title, claim) => `The argument around "${title}" ends wherever someone finally admits that ${claim}.`,
  (title, claim) => `A plaque may eventually misdescribe "${title}". For now, the useful truth is that ${claim}.`,
  (title, claim) => `Nothing about "${title}" is accidental once you notice that ${claim}.`
];

function codaVariant(id) {
  return hash32(id + ":coda") % codaTemplates.length;
}

function materialCoda(event) {
  const frames = themes[event.theme].frames;
  const frame = frames[hash32(event.id) % frames.length].replace(/\.$/, "");
  const claim = frame[0].toLowerCase() + frame.slice(1);
  const title = caseTitle(event.id);
  return codaTemplates[codaVariant(event.id)](title, claim);
}

function allOutcomeLists(choice) {
  return [choice.outcomes, choice.success, choice.fail].filter(Array.isArray);
}

function addPoliticalEffect(choice) {
  const effect = politicalEffects[choice.approach];
  if (!effect) return;
  for (const outcomes of allOutcomeLists(choice)) {
    for (const outcome of outcomes) {
      const effects = Array.isArray(outcome.effects) ? outcome.effects : [];
      const alreadyPersistent = effects.some(value => /^(region|region_flag|region_spread|collective|rumor|foreshadow|agenda|npc_rel|network|schedule) /.test(value));
      if (!alreadyPersistent) effects.push(effect);
      outcome.effects = effects;
    }
  }
}

const counts = Object.fromEntries(Object.keys(themes).map(name => [name, 0]));
for (const file of manifest.files) {
  if (!file.startsWith("r16_") && !file.startsWith("r17_")) continue;
  const events = JSON.parse(fs.readFileSync(path.join(eventsDir, file), "utf8"));
  for (const event of events)
    if (event.theme && counts[event.theme] !== undefined) counts[event.theme]++;
}

let remastered = 0;
let tagged = 0;
for (const file of manifest.files) {
  const filePath = path.join(eventsDir, file);
  const events = JSON.parse(fs.readFileSync(filePath, "utf8"));
  let changed = false;
  for (const event of events) {
    if (!event.primary) {
      event.primary = eventPrimary(event);
      changed = true;
    }
    const isModern = file.startsWith("r16_") || file.startsWith("r17_");
    const wasLegacyRemaster = event.remaster === "v17";
    if (isModern && wasLegacyRemaster) {
      const split = event.text.lastIndexOf("\n\n");
      if (split >= 0) event.text = event.text.slice(0, split);
      delete event.remaster;
      delete event.remaster_layout;
      changed = true;
    }
    if (!isModern) {
      if (wasLegacyRemaster) {
        if (event.remaster_layout === "suffix") {
          const split = event.text.lastIndexOf("\n\n");
          if (split >= 0) event.text = event.text.slice(0, split);
        } else {
          const split = event.text.indexOf("\n\n");
          if (split >= 0) event.text = event.text.slice(split + 2);
        }
      }
      event.theme = chooseTheme(event, counts);
      counts[event.theme]++;
      changed = true;
      if (!wasLegacyRemaster && file !== "politics.json") tagged++;
    } else if (!event.theme) {
      event.theme = chooseTheme(event, counts);
      counts[event.theme]++;
      changed = true;
      tagged++;
    }
    const tags = new Set((Array.isArray(event.tags) ? event.tags : [])
      .filter(tag => isModern || (themes[tag] === undefined && tag !== "political" &&
        !tag.startsWith("voice_"))));
    tags.add(event.theme);
    if (!isModern && file !== "politics.json") tags.add(`voice_${codaVariant(event.id)}`);
    if (file.startsWith("r17_")) tags.add("political");
    event.tags = [...tags];

    for (let i = 0; i < (event.choices || []).length; i++) {
      const choice = event.choices[i];
      if (!choice.approach) choice.approach = inferApproach(choice.text, i);
      addPoliticalEffect(choice);
    }

    if (!isModern && file !== "politics.json") {
      event.text = event.text + "\n\n" + materialCoda(event);
      event.remaster = "v17";
      event.remaster_layout = "suffix";
      if (!wasLegacyRemaster) remastered++;
      changed = true;
    }
  }
  if (changed) fs.writeFileSync(filePath, JSON.stringify(events, null, 2) + "\n", "utf8");
}

console.log(`legacy remaster: ${remastered} rewritten, ${tagged} newly themed`);
for (const name of Object.keys(themes).sort())
  console.log(`${name}: ${counts[name]}`);
