import fs from "node:fs";
import path from "node:path";

const assets = process.argv[2] || "assets";
const eventsDir = path.join(assets, "data", "events");
const outputName = "r18_living_politics.json";
const manifestPath = path.join(eventsDir, "manifest.json");
const targetsPath = path.join(assets, "data", "scenario_targets.json");
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const targets = JSON.parse(fs.readFileSync(targetsPath, "utf8"));
const sourceFiles = manifest.files.filter((name) => name !== outputName);
const existing = sourceFiles.flatMap((name) =>
  JSON.parse(fs.readFileSync(path.join(eventsDir, name), "utf8"))
);

const rawFamilies = [
  "dock_ledger|the dockworkers' pay-ledger circle|the Harbor Efficiency Trust|every hour of unpaid loading|the public cranes|union|a ceremonial clipboard with hereditary authority|a harbor where nobody rents their own hands back",
  "tenant_tower|the tenants of Lantern Tower|Baron Vacant Holdings|the building's inhabited floors|the master keys|tenant_council|an eviction trumpet that only plays indoors|homes governed by the people living in them",
  "refinery_river|the river testing club|Bright Tomorrow Petrochemicals|a river that now catches fire politely|the water table|restoration_crew|a fish serving as its own expert witness|clean water without a premium tier",
  "free_kitchen|the midnight kitchen collective|the Office of Licensed Hunger|three neighborhoods' dinners|the community ovens|mutual_aid|a soup inspector measuring rebellion by ladle|food offered because people are hungry",
  "veterans_peace|the veterans against the next war|General Market Opportunity|a recruitment drive disguised as mourning|the memorial square|peace_movement|a medal awarded for attending the medal meeting|peace treated as work instead of weather",
  "archive_names|the keepers of the names|the Ministry of Necessary Forgetting|the registry of vanished families|the public archive|scientific_commons|a censor allergic to alphabetical order|every victim remembered as a person",
  "museum_return|the descendants' return delegation|the Imperial Museum of Convenient Discovery|stolen household gods and tools|the locked collection|scientific_commons|a curator who says ownership expired during shipping|history returned to the people who survived it",
  "oxygen_patent|the breathers' defense league|Universal Air Ventures|the atmosphere's new subscription model|the oxygen works|cooperative|a meter that charges extra for sighing|air remaining too common to own",
  "ration_algorithm|the ration-card debuggers|the Office of Objective Need|an algorithm that feeds surnames before bodies|the food servers|scientific_commons|a machine demanding proof of continued existence|abundance distributed by need",
  "postponement_committee|the emergency action caucus|the Committee for Studying Immediate Action|a fascist march scheduled during consultation|the council chamber|antifascist_coalition|a motion to define the word motion|a coalition that votes and also acts",
  "checkpoint_law|the checkpoint witnesses|the Civic Reassurance Guard|disappearances filed as traffic control|the crossing records|antifascist_coalition|a barrier asking travelers to arrest themselves|public safety without public terror",
  "monster_local|the dungeon monsters' bargaining local|Heroic Delving Incorporated|hazard pay for being stabbed by tourists|the dungeon entrance|union|a hydra requiring one vote per head|monsters treated as workers and persons",
  "failed_commune|the survivors of the seventh commune|the Bureau of Permanent Victory|a revolution embarrassed by its own offices|the common hall|revolutionary_committee|a chairman's chair that keeps electing itself|another attempt wiser than the last",
  "pirate_constitution|the deckhands' floating assembly|Admiral Sole Share|the captaincy's claim to every horizon|the ship's wheel|pirate_assembly|a parrot objecting on constitutional grounds|a ship whose course belongs to its crew",
  "cryptid_consent|the field naturalists' commons|Professor Capture First|the last moon-winged witness|the observation notebooks|scientific_commons|a butterfly net with an ethics appendix|knowledge gained without turning life into property",
  "demon_clinic|the night clinic volunteers|the Order of Correctly Feared Creatures|a feverish demon denied a person-number|the medicine cabinet|mutual_aid|the demon's mother correcting everyone's bedside manners|care offered before metaphysics",
  "ministry_denial|the printers of observable facts|the Ministry of Clear Skies|industrial smoke officially classified as enthusiasm|the public press|scientific_commons|a spokesman denying the podium beneath him|truth becoming harder to purchase",
  "refugee_ferry|the volunteer ferry crews|the Border Investment Authority|families stranded between profitable jurisdictions|the river boats|mutual_aid|a customs goose demanding three identical bread forms|movement treated as a human need",
  "arms_factory|the shell-factory night shift|Peace Through Quarterly Growth|workers ordered to manufacture their own mourning|the assembly line|union|a bomb stamped HANDLE WITH DIPLOMACY|workers converting war production to useful work",
  "mine_rescue|the deep-miners' rescue council|Bottomless Extraction Limited|a collapsed shaft and a missing crew|the lift machinery|union|a canary promoted to regional manager|no ore priced above a miner's life",
  "seed_vault|the seed librarians|AgriFuture Patent Cathedral|the last unlicensed crop varieties|the seed vault|scientific_commons|a turnip accused of intellectual theft|food knowledge held in common",
  "moon_landlord|the crater tenants' association|Lunar Shelter Assets|rent charged for shadows and vacuum|the pressure domes|tenant_council|a lease defining gravity as an amenity|housing on the moon without landlords",
  "police_clock|the families of the detained|the Clockwork Constabulary|an arrest schedule written before any crimes|the station clock|antifascist_coalition|a judge's wig issuing independent warrants|law answerable to the people it touches",
  "parade_route|the neighborhood defense choir|the League of Identical Boots|a parade rehearsing tomorrow's dictatorship|the public avenue|antifascist_coalition|a goose-stepping permit clerk lost in a roundabout|fascists denied both power and good staging",
  "desalination|the coast's water council|BlueGold Utility Partners|a drought beside a privatized ocean|the desalination pumps|restoration_crew|a tariff on unauthorized rain|clean water governed as a commons",
  "haunted_warehouse|the warehouse ghosts' association|Afterlife Logistics|centuries of unpaid spectral labor|the loading bays|union|a ghost required to clock out before haunting|the dead finally receiving back pay",
  "robot_redundancy|the obsolete robots' repair circle|Human Authenticity Holdings|machines discarded after creating abundance|the repair manuals|cooperative|a toaster delivering a redundancy speech|persons valued beyond productivity",
  "healer_strike|the exhausted healers' union|MercyCare Revenue Temple|a hospital charging admission to the dying|the medicine stores|union|a surgeon billing the concept of Tuesday|care organized around life",
  "school_sale|the parents and apprentice teachers|FutureMinds Private Academy|a school sold with the students still inside|the classrooms|cooperative|a tuition invoice requiring advanced arithmetic|education treated as public wealth",
  "forest_commons|the forest villages' assembly|Royal Timber Destiny|ancestral woods converted into inventory|the old paths|restoration_crew|a tree cited for unlawful standing|land protected by those who depend on it",
  "swamp_pipeline|the marsh restoration camp|Continental Pipe and Apology|a pipeline crossing the spawning pools|the reed beds|restoration_crew|a safety mascot shaped like a dead frog|the swamp surviving the quarterly report",
  "mountain_observatory|the public astronomers|Celestial Data Exclusives|a sky placed behind a paywall|the observatory lenses|scientific_commons|a comet refusing the premium viewing window|the universe remaining public",
  "debt_jubilee|the tavern debt circle|Saint Compound Interest|generations of inherited bar tabs|the account books|cooperative|a calculator demanding collateral from a sandwich|debts abolished before debtors",
  "open_prison|the families at the prison gate|Corrections Growth Group|a jail expanding faster than the town|the cell keys|antifascist_coalition|a rehabilitation rack with adjustable guilt|safety built without cages for profit",
  "strike_press|the striking printers|The Neutral Daily Concern|a newspaper hiding the strike printing it|the presses|union|an editorial insisting ink has no politics|workers owning the story of their work",
  "border_census|the census refusal network|the Office of Pure Lineage|a registry designed for tomorrow's roundup|the census ledgers|antifascist_coalition|a blood-quantum abacus that cannot count itself|identity protected from exterminating power",
  "mercenary_peace|the demobilized mercenaries|EverWar Solutions|a peace treaty threatening annual revenue|the weapons depots|peace_movement|a sword filing for hostile unemployment|peace surviving its investors",
  "royal_succession|the commoners' constitutional picnic|the Crown Continuity Fund|a throne searching for another occupant|the palace gates|revolutionary_committee|a crown interviewing candidates|freedom outliving royal branding",
  "gentle_schism|the congregation of practical mercy|the Synod of Correct Spoons|a doctrinal war over feeding strangers|the shared sanctuary|mutual_aid|a holy spoon with disputed jurisdiction|faith measured by care rather than obedience",
  "plague_market|the bedside mutual-aid crews|Pestilence Opportunity Partners|medicine hoarded during an outbreak|the clinic stores|mutual_aid|a vulture in a waistcoat offering funeral futures|treatment reaching people before investors",
  "flood_housing|the floodplain tenant boats|DryLand Portfolio Group|evacuees charged rent for rescue rafts|the high-ground shelters|tenant_council|a landlord claiming ownership of the tide|disaster relief without extraction",
  "company_colony|the colony workers' congress|Founder's Planet Company|a whole moon governed by an employment contract|the landing charter|cooperative|a flag planted in someone else's lunch|a colony becoming a community instead of an asset",
  "portal_timeshare|the interdimensional tenants|Everywhere Estates|the same apartment rented in six realities|the portal keys|tenant_council|a landlord collecting rent from alternate selves|housing obeying one reality and common sense",
  "union_election|the rank-and-file slate|the Permanent Interim Leadership|a union election postponed for stability|the ballot box|union|a ballot demanding a reference from management|democracy continuing after the meeting",
  "coop_capture|the cooperative's night assembly|Friendly Consolidation Group|a worker co-op offered a profitable surrender|the ownership ledger|cooperative|a consultant explaining cooperation without workers|shared ownership surviving success",
  "revolution_office|the street committees|the Central Office of Spontaneity|a revolution developing a permit department|the assembly hall|revolutionary_committee|a barricade requiring a queue ticket|liberation resisting its own administrators",
  "peacekeeper_occupation|the occupied neighborhood councils|the Benevolent Security Mission|peacekeepers extending peace by extending occupation|the checkpoint map|peace_movement|a tank wearing a visitor badge|peace without foreign control",
  "grain_futures|the village grain league|Harvest Futures Exchange|bread prices rising before the wheat exists|the granaries|cooperative|a scarecrow trading derivatives|food leaving the casino",
  "water_council|the watershed assemblies|Aqua Crown Authority|upstream power deciding downstream thirst|the sluice gates|restoration_crew|a river ordered to present identification|water governed across borders",
  "public_transit|the drivers and stranded riders|RoadChoice Conglomerate|a transit system dismantled for efficiency|the bus depot|union|a timetable listing only profitable minutes|movement organized as public freedom",
  "ship_mutiny|the galley and engine crews|Captain Final Word|a voyage where orders outnumber rations|the navigation charts|pirate_assembly|a mast requesting recognition as middle management|the crew choosing both captain and destination",
  "asteroid_mine|the vacuum miners' local|Meteor Wealth Extraction|oxygen deducted from miners' wages|the air locks|union|a spacesuit repossession agent|space labor breathing freely",
  "alien_embassy|the translators' welcome circle|the Protocol Preservation Office|first contact trapped in a seating dispute|the translation table|scientific_commons|an ambassador who communicates exclusively through soup|solidarity crossing species before paperwork",
  "clone_personhood|the clone mutual-recognition league|Original Persons Incorporated|people invoiced for resembling patented donors|the identity records|scientific_commons|a mirror called as a hostile witness|personhood belonging to every person",
  "memory_market|the keepers of unbought memories|Nostalgia Capital|childhood memories bundled into securities|the recording vault|scientific_commons|a broker short-selling embarrassment|memory remaining part of the self",
  "funeral_insurance|the bereaved families' circle|LastChance Assurance|grief denied for a preexisting death|the funeral fund|mutual_aid|an actuary surprised by mortality|mourning freed from invoices",
  "language_ban|the underground teachers|the Office of One Acceptable Tongue|a language criminalized one lullaby at a time|the hidden school|antifascist_coalition|a grammar inspector arrested by an irregular verb|culture surviving forced silence",
  "public_library|the librarians' defense brigade|Knowledge Access Premium|books converted into timed rentals|the public stacks|scientific_commons|a late fee older than the kingdom|knowledge remaining available without permission",
  "universal_repair|the wandering repair commons|Planned Obsolescence Saints|tools designed to die before their owners|the repair manuals|cooperative|a warranty expiring during the sentence|abundance maintained instead of discarded"
];

const families = rawFamilies.map((line) => {
  const [id, collective, antagonist, crisis, resource, institution, absurdity, promise] = line.split("|");
  return { id, collective, antagonist, crisis, resource, institution, absurdity, promise };
});
if (families.length !== 59) throw new Error(`expected 59 families, found ${families.length}`);

const endingSpecs = [
  { id: "common_future", theme: "labor", primary: "city", when: ["day > 30", "world worker_power >= 5", "world solidarity >= 5", "world wealth_concentration <= 0"], title: "THE COMMON FUTURE", text: "Work still exists, but nobody can buy another person's hunger. The councils are noisy, the trains are late, and every official chair can be recalled by the people forced to look at it.", epitaph: "Helped make ordinary life belong to ordinary people." },
  { id: "another_attempt", theme: "revolution", primary: "road", when: ["day > 30", "world worker_power >= 4", "institution revolutionary_committee >= 4", "world fascist_influence <= 0"], title: "ANOTHER ATTEMPT", text: "The old revolution failed. This one has read its minutes, distrusted its heroes, and packed lunch. Nobody promises permanence. Everyone promises to keep watch over power, including their own.", epitaph: "Joined the revolution that expected to be corrected." },
  { id: "broad_front_holds", theme: "antifascist", primary: "tavern", when: ["day > 28", "world fascist_influence <= -4", "institution antifascist_coalition >= 4"], title: "THE BROAD FRONT HOLDS", text: "The fascists are defeated by an alliance that immediately resumes arguing about taxes, banners, and soup viscosity. The arguments are exhausting. The absence of mass graves is not.", epitaph: "Helped a quarrelsome coalition stop the boots." },
  { id: "demobilized_stars", theme: "antiwar", primary: "coast", when: ["day > 30", "world militarization <= -4", "institution peace_movement >= 4"], title: "DEMOBILIZED STARS", text: "The warships become ferries, hospitals, and one aggressively overarmed public library. Admirals complain that peace lacks promotion pathways. Nobody is required to solve that problem.", epitaph: "Helped turn the fleet toward home." },
  { id: "waters_return", theme: "ecology", primary: "forest", when: ["day > 30", "world pollution <= -4", "institution restoration_crew >= 4"], title: "THE WATERS RETURN", text: "The rivers clear slowly, which is how rivers do almost everything. The companies call restoration unrealistic until the fish return without seeking investor approval.", epitaph: "Stayed long enough to see the river remember itself." },
  { id: "abundance_without_owners", theme: "post_scarcity", primary: "crash", when: ["day > 32", "world food_security >= 5", "world wealth_concentration <= 0", "institution scientific_commons >= 4"], title: "ABUNDANCE WITHOUT OWNERS", text: "The machines can feed everyone. This time, nobody invents an artificial shortage to preserve character. Humanity keeps three landlords in a museum where they explain scarcity to baffled children.", epitaph: "Helped abundance escape its owners." },
  { id: "naive_power", theme: "kindness", primary: "city", when: ["day > 30", "world mutual_aid >= 6", "world solidarity >= 6", "collective >= 3"], title: "THE NAIVE POWER", text: "Kindness did not save everyone. It saved enough people to build clinics, kitchens, rescue ships, and another chance. The cynics remain technically correct and completely outnumbered at dinner.", epitaph: "Kept trying the naive thing until it became infrastructure." }
];

const counts = (field) => {
  const out = {};
  for (const event of existing) out[event[field]] = (out[event[field]] || 0) + 1;
  return out;
};
const deckCount = counts("primary");
const themeCount = counts("theme");
for (const ending of endingSpecs) {
  deckCount[ending.primary] = (deckCount[ending.primary] || 0) + 1;
  themeCount[ending.theme] = (themeCount[ending.theme] || 0) + 1;
}

function balancedSchedule(current, quotas, length, excluded = new Set()) {
  const result = [];
  const last = [];
  for (let i = 0; i < length; i++) {
    let candidates = Object.keys(quotas).filter((key) => !excluded.has(key));
    const cool = candidates.filter((key) => !last.slice(-2).includes(key));
    if (cool.length) candidates = cool;
    candidates.sort((a, b) => {
      const needA = (quotas[a] || 0) - (current[a] || 0);
      const needB = (quotas[b] || 0) - (current[b] || 0);
      if (needA !== needB) return needB - needA;
      return (current[a] || 0) - (current[b] || 0) || a.localeCompare(b);
    });
    const pick = candidates[0];
    result.push(pick);
    current[pick] = (current[pick] || 0) + 1;
    last.push(pick);
  }
  return result;
}

const themeSchedule = balancedSchedule(themeCount, targets.themes, 472);
const deckSchedule = balancedSchedule(deckCount, targets.decks, 472, new Set(["special"]));

const beatNames = ["petition", "assembly", "retaliation", "coalition", "crisis", "compromise", "reversal", "legacy"];
const leadSets = [
  ["At first bell", "Before breakfast", "Under public notice", "During a routine emergency", "At the smallest meeting", "Before the officials arrive", "On an ordinary morning", "At the edge of patience"],
  ["By second bell", "Inside the crowded hall", "After three arguments", "With every chair occupied", "Beneath improvised banners", "During the open assembly", "Before anyone adjourns", "At the first real vote"],
  ["Two days later", "Management answers", "Authority develops feelings", "The investors retaliate", "A uniformed memo arrives", "The polite threats begin", "At closing time", "The counterattack wears a tie"],
  ["The coalition meets", "Unexpected allies arrive", "The broad front narrows", "Compromise requests a chair", "At the unity banquet", "The moderates bring folders", "The radicals bring lunch", "Everyone shares one microphone"],
  ["Then the machinery fails", "At the worst hour", "The crisis becomes physical", "A siren interrupts theory", "The floor begins moving", "Someone opens the wrong gate", "The shortage becomes visible", "Reality rejects the agenda"],
  ["A bargain appears", "The respectable offer arrives", "A compromise wears perfume", "The settlement has footnotes", "An envoy offers calm", "The price of peace speaks", "A practical surrender circulates", "The middle path invoices everyone"],
  ["The hidden ledger opens", "A trusted voice defects", "The absurdity confesses", "The chair votes alone", "A witness changes sides", "The private minutes leak", "The mascot removes its head", "The final excuse collapses"],
  ["Years seem to pass", "At the final assembly", "When the banners fade", "After the cheering stops", "The morning after victory", "The morning after failure", "Once the chairs are stacked", "When ordinary life returns"]
];
const beatBodies = [
  (f) => `${f.collective} discovers that ${f.antagonist} has made ${f.crisis} official policy. The dispute centers on ${f.resource}. ${f.absurdity} has been appointed to keep the process dignified.`,
  (f) => `${f.collective} calls an assembly around ${f.resource}. Every person speaks, including one who is clearly three children in a ceremonial coat. The proposal is simple: organize before ${f.antagonist} can rename the problem.`,
  (f) => `${f.antagonist} answers collective action with permits, hired concern, and a professionally printed rumor. ${f.crisis} is now described as a consumer preference. ${f.absurdity} signs the explanation.`,
  (f) => `${f.collective} gains allies who agree on the emergency and almost nothing else. One faction wants immediate action, another wants a correctly stapled moral victory, and ${f.antagonist} wants everyone to keep discussing both.`,
  (f) => `${f.crisis} stops being abstract. People need ${f.resource} before sunset. ${f.absurdity} offers a technically legal solution that would save the paperwork and lose the people.`,
  (f) => `${f.antagonist} offers a settlement: limited relief, permanent control, and a plaque thanking itself. The offer could prevent immediate harm. It could also teach power exactly how little resistance costs.`,
  (f) => `The private ledger reveals who profited from ${f.crisis}. It also reveals that a respected ally accepted one very comfortable exception. ${f.collective} must decide whether accountability is a weapon, a practice, or a public performance.`,
  (f) => `${f.collective} stands beside ${f.resource} and counts what survived. The institutions are imperfect, the loudest hero is already writing a memoir, and ${f.promise} is finally possible if people keep doing the unglamorous work.`
];
const jokes = ["ceremonial", "bureaucratic", "cosmic", "literalism", "polite_monster", "administrative", "pirate_logic", "committee" ];
const roles = ["workers", "tenants", "caregivers", "witnesses", "refugees", "scientists", "neighbors", "crew"];
const shapes = ["collective_reform_exit", "organize_investigate_profit", "care_law_force", "commons_hearing_buyout", "strike_document_defect", "rescue_negotiate_exploit", "abolish_regulate_own", "solidarity_delay_private_deal"];

const themePositive = {
  labor: "world worker_power +1", housing: "world rent_burden -1", antifascist: "world fascist_influence -1",
  antiwar: "world militarization -1", anti_genocide: "world fascist_influence -1", ecology: "world pollution -1",
  care: "world mutual_aid +1", food: "world food_security +1", colonialism: "world wealth_concentration -1",
  capital: "world wealth_concentration -1", post_scarcity: "world food_security +1", liberalism: "world legitimacy +1",
  state_power: "world militarization -1", union: "world worker_power +1", revolution: "world solidarity +1",
  freedom: "world militarization -1", science: "world legitimacy +1", faith: "world mutual_aid +1",
  propaganda: "world legitimacy +1", kindness: "world mutual_aid +1"
};
const themeNegative = {
  labor: "world worker_power -1", housing: "world rent_burden +1", antifascist: "world fascist_influence +1",
  antiwar: "world militarization +1", anti_genocide: "world fascist_influence +1", ecology: "world pollution +1",
  care: "world mutual_aid -1", food: "world food_security -1", colonialism: "world wealth_concentration +1",
  capital: "world wealth_concentration +1", post_scarcity: "world wealth_concentration +1", liberalism: "world legitimacy -1",
  state_power: "world militarization +1", union: "world worker_power -1", revolution: "world solidarity -1",
  freedom: "world militarization +1", science: "world legitimacy -1", faith: "world mutual_aid -1",
  propaganda: "world legitimacy -1", kindness: "world mutual_aid -1"
};

function stageEffects(family, beat, mode) {
  const effects = [];
  if (beat === 0) effects.push(`story +r18_${family.id}_begun`);
  else effects.push(`story -r18_${family.id}_stage_${beat}`);
  if (beat < 7) {
    effects.push(`story +r18_${family.id}_stage_${beat + 1}`);
    const days = 2 + ((families.indexOf(family) + beat) % 4);
    effects.push(`schedule ${days} r18_${family.id}_${beatNames[beat + 1]} ${family.collective} will need you again`);
  } else {
    effects.push(`story +r18_${family.id}_resolved`);
    effects.push(`converge r18_${family.id}`);
  }
  if (mode === 0) {
    effects.push(beat === 0 ? `join ${family.institution}` : `standing ${family.institution} +1`);
    if (beat === 1) effects.push(`tendency ${family.institution} rank_and_file`);
    if (beat === 7) effects.push(`role ${family.institution} +1`);
  } else if (mode === 1) {
    if (beat > 0) effects.push(`standing ${family.institution} +1`);
    if (beat === 1) effects.push(`tendency ${family.institution} broad_front`);
  } else {
    effects.push(`standing ${family.institution} -1`);
    if (beat === 1) effects.push(`tendency ${family.institution} direct_action`);
  }
  return effects;
}

function makeChoice(family, beat, theme, mode) {
  const stage = stageEffects(family, beat, mode);
  const pattern = (families.indexOf(family) * 3 + beat) % 6;
  if (mode === 0) {
    const success = {
      text: `${family.collective} does the slow work together. Nobody becomes a legend, which leaves more room for everyone to become capable. ${family.promise} moves one difficult step closer.`,
      effects: ["world solidarity +1", themePositive[theme], `institution ${family.institution} +1`, "region solidarity +1", ...stage]
    };
    const failure = {
      text: `The organizing attempt is public, awkward, and incomplete. ${family.antagonist} laughs too early. The network survives, but so does the immediate danger.`,
      effects: ["hp -1", "region unrest +1", `institution ${family.institution} +1`, ...stage]
    };
    const choice = {
    text: `Put ${family.resource} under common control with ${family.collective}`,
    approach: "solidarity",
    };
    if (pattern <= 1) {
      choice.check = { stat: pattern === 0 ? "cha" : "wis", dc: 11 + (beat % 4) };
      choice.success = [success];
      choice.fail = [failure];
    } else {
      if (pattern === 3) success.effects.unshift("money -3");
      choice.outcomes = [
        { ...success, when: [`institution ${family.institution} >= 4`, `member ${family.institution}`], effects: [...success.effects, "world mutual_aid +1"] },
        { ...success, when: [`institution ${family.institution} >= 4`] },
        failure
      ];
    }
    return choice;
  }
  if (mode === 1) {
    const success = {
      text: `${family.antagonist} is required to answer in public. The answer is terrible, but now it belongs to the record instead of the rumor mill. Reform buys time and leaves the ownership question glaring.`,
      effects: ["world legitimacy +1", `institution ${family.institution} +1`, "region pressure -1", ...stage]
    };
    const failure = {
      text: `The hearing produces a recommendation to schedule a hearing. ${family.absurdity} receives a commendation. The delay protects some people and exhausts the rest.`,
      effects: ["world legitimacy -1", "region pressure +1", ...stage]
    };
    const choice = {
    text: `Force a public hearing on ${family.crisis}`,
    approach: beat % 2 ? "wit" : "law",
    };
    if (pattern === 2 || pattern === 4) {
      choice.check = { stat: pattern === 2 ? "int" : "cha", dc: 12 + (beat % 3) };
      choice.success = [success];
      choice.fail = [failure];
    } else {
      if (pattern === 5) choice.requires = { money: 6 };
      choice.outcomes = [
        { ...success, when: ["world legitimacy >= 3", `role ${family.institution} >= 2`], effects: [...success.effects, themePositive[theme], `standing ${family.institution} +1`] },
        { ...success, when: ["world legitimacy >= 3"], effects: [...success.effects, themePositive[theme]] },
        failure
      ];
    }
    return choice;
  }
  if (pattern === 5) return {
    text: `Sabotage ${family.antagonist} before anyone approves it`,
    approach: "force",
    check: { stat: "dex", dc: 13 },
    success: [{
      text: `The machinery stops with a noise like a board meeting falling downstairs. ${family.collective} gains time, not safety.`,
      effects: [themePositive[theme], "world solidarity +1", "region danger +1", ...stage]
    }],
    fail: [{
      text: `The sabotage becomes a very short public tour. You escape with bruises while ${family.antagonist} acquires a martyrdom budget.`,
      effects: ["hp -3", "world militarization +1", "region pressure +1", ...stage]
    }]
  };
  const privateSuccess = {
    text: `${family.antagonist} pays promptly and calls the transaction peace. ${family.absurdity} witnesses the receipt. The immediate crisis softens for you and hardens for everyone else.`,
    effects: ["money +8", themeNegative[theme], "world wealth_concentration +1", "world solidarity -1", "region unrest +1", ...stage]
  };
  if (pattern === 2) return {
    text: `Read the private offer before deciding whom it serves`,
    approach: "curiosity",
    check: { stat: "wis", dc: 12 },
    success: [{ text: `The hidden clause is the whole agreement. You publish it and keep the advance as evidence.`, effects: ["money +4", "world legitimacy +1", themePositive[theme], ...stage] }],
    fail: [privateSuccess]
  };
  return {
    text: `Take the private offer concerning ${family.resource}`,
    approach: "greed",
    outcomes: [
      { ...privateSuccess, when: ["world wealth_concentration >= 5"], effects: [...privateSuccess.effects, "money +6", "world fascist_influence +1"] },
      privateSuccess
    ]
  };
}

const generated = [];
let cursor = 0;
for (let fi = 0; fi < families.length; fi++) {
  const family = families[fi];
  for (let beat = 0; beat < 8; beat++) {
    const theme = themeSchedule[cursor];
    const primary = deckSchedule[cursor];
    const variant = (fi * 3 + beat * 5) % 8;
    const id = `r18_${family.id}_${beatNames[beat]}`;
    const when = beat === 0
      ? [`!story r18_${family.id}_begun`]
      : [`story r18_${family.id}_stage_${beat}`];
    generated.push({
      id,
      primary,
      theme,
      locations: [primary],
      weight: beat === 0 ? 7 : 13,
      family: `r18_${family.id}`,
      tags: [theme, "living_politics", family.institution, `beat_${beatNames[beat]}`, `voice_${fi % 12}`],
      fingerprints: [`opening_${beatNames[beat]}_${variant}`, `joke_${jokes[fi % jokes.length]}`, `role_${roles[fi % roles.length]}`, `shape_${shapes[(fi + beat) % shapes.length]}`],
      when,
      text: `${family.id.replaceAll("_", " ").toUpperCase()}\n\n${leadSets[beat][variant]}. ${beatBodies[beat](family)}`,
      choices: [0, 1, 2].map((mode) => makeChoice(family, beat, theme, (mode + fi + beat) % 3))
    });
    cursor++;
  }
}

for (let i = 0; i < endingSpecs.length; i++) {
  const ending = endingSpecs[i];
  generated.push({
    id: `r18_ending_${ending.id}`,
    primary: ending.primary,
    theme: ending.theme,
    locations: [ending.primary],
    weight: 18,
    family: "r18_ideological_endings",
    tags: [ending.theme, "ending", "living_politics", `voice_${i}`],
    fingerprints: [`opening_ending_${i}`, "joke_humane_aftermath", "role_world", `shape_ending_${i}`],
    when: [`ending_ready ${ending.id}`],
    text: `${ending.title}\n\n${ending.text}`,
    choices: [
      { text: "Let this life become part of the new world", approach: "solidarity", outcomes: [{ text: "You stop wandering. The work does not stop with you.", effects: [`finish ${ending.epitaph}`] }] },
      { text: "Keep walking. A finished world is usually lying", approach: "curiosity", outcomes: [{ text: "You leave the celebration before anyone can name a building after you.", effects: ["world legitimacy +1", "world solidarity +1"] }] }
    ]
  });
}

if (generated.length !== 479) throw new Error(`expected 479 generated events, found ${generated.length}`);
if (existing.length + generated.length !== 1000) throw new Error(`expected 1000 total events, found ${existing.length + generated.length}`);
fs.writeFileSync(path.join(eventsDir, outputName), JSON.stringify(generated, null, 2) + "\n");
if (!manifest.files.includes(outputName)) manifest.files.push(outputName);
fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + "\n");

const finalEvents = [...existing, ...generated];
const finalThemes = {};
const finalDecks = {};
for (const event of finalEvents) {
  finalThemes[event.theme] = (finalThemes[event.theme] || 0) + 1;
  finalDecks[event.primary] = (finalDecks[event.primary] || 0) + 1;
}
console.log(`Release 18 generated ${generated.length} connected cards across ${families.length} families and ${endingSpecs.length} endings.`);
console.log(`Total scenarios: ${finalEvents.length}`);
console.log(JSON.stringify({ decks: finalDecks, themes: finalThemes }, null, 2));
