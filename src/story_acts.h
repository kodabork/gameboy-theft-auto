/* Acts Two to Four of STORY.md: missions 10-40 as step lists for the mission engine in game.c.
   Included once, from game.c. Coordinates in metres; a city block is 64 m (see CLAUDE.md, Map).
   Dialogue lines are at most 35 characters. Speakers: NICK, MARTA, JINX, RUDY, BOHDAN, DEE, KANE,
   PRYCE, MERCER. Models: Marta and Dee have their own women's models; Jinx green, Rudy white, Bohdan his own older-man model, Kane and Pryce
   suit, Mercer and the Blackline guards in uniform. */

#define AT(bx, by, ox, oy) (bx) * 64 + (ox), (by) * 64 + (oy)
#define NS(bx, by) AT(bx, by, 40, 13)      /* north sidewalk of a block */
#define ES(bx, by) AT(bx, by, 62, 44)      /* east sidewalk (clear of shop doors) */
#define SS(bx, by) AT(bx, by, 40, 62)      /* south sidewalk */
#define WS(bx, by) AT(bx, by, 13, 40)      /* west sidewalk */
#define KERB(bx, by) AT(bx, by, 11, 32)    /* kerb lane of the road on the block's west side */
#define ROADN(bx, by) AT(bx, by, 40, 6)    /* the road on the block's north side */
#define DYN NOWHERE, 0                     /* where the last car stopped or target fell */
#define SHOPPOS 79, 424                    /* Bohdan's Auto */
#define SAFEPOS AT(5, 12, 40, 6)           /* the Heights safehouse */

/* step helpers: { op, n, flags, arg, model, x, y, t, line 1, line 2 } */
#define S_GO(fl, pos, t, a, b)            { OP_GO, 0, fl, 0, 0, pos, t, a, b }
#define S_CAR(model, pos, hd, a, b)       { OP_CAR, 0, 0, hd, model, pos, 0, a, b }
#define S_KEEPCAR(a, b)                   { OP_CAR, 0, F_KEEP, 0, 0, 0, 0, 0, a, b }
#define S_DROP(fl, pos, t, a, b)          { OP_DROP, 0, fl, 0, 0, pos, t, a, b }
#define S_KILL(n, fl, model, pos, hp, a, b) { OP_KILL, n, fl, 0, model, pos, hp, a, b }
#define S_WRECK(n, model, pos, guards, uni, a, b) { OP_WRECK, n, 0, guards, model, pos, uni, a, b }
#define S_CHASE(fl, model, pos, far, a, b) { OP_CHASE, 0, fl, 0, model, pos, far, a, b }
#define S_TAIL(model, pos, secs, a, b)    { OP_TAIL, 0, 0, 0, model, pos, secs, a, b }
#define S_DEFEND(waves, model, pos, a, b) { OP_DEFEND, waves, 0, 0, model, pos, 0, a, b }
#define S_HUNT(n, fl, model, a, b)        { OP_HUNT, n, fl, 0, model, 0, 0, 0, a, b }
#define S_HEAT(n, a, b)                   { OP_HEAT, n, 0, 0, 0, 0, 0, 0, a, b }
#define S_SURVIVE(n, secs, a, b)          { OP_SURVIVE, n, 0, 0, 0, 0, 0, secs, a, b }
#define S_SPOT(pos, a, b)                 { OP_SPOT, 0, 0, 0, 0, pos, 0, a, b }
#define S_SAY(k)                          { OP_SAY, k, 0, 0, 0, 0, 0, 0, 0, 0 }
#define S_GIVE(w, ammo)                   { OP_GIVE, w, 0, 0, 0, 0, 0, ammo, 0, 0 }
#define S_BOSS(model, guards, uni, pos, hp, a, b) { OP_BOSS, guards, 0, uni, model, pos, hp, a, b }
#define S_CHOICE(flag, model, pos, a, b)  { OP_CHOICE, flag, 0, 0, model, pos, 0, a, b }
#define S_PICKUP(model, pos, t, a, b)     { OP_PICKUP, 0, 0, 0, model, pos, t, a, b }
#define NSTEPS(a) (u8)(sizeof(a) / sizeof((a)[0]))
#define MDEF(title, giver, pos, intro, steps, pay) { title, giver, pos, intro, NPAGES(intro), NSTEPS(steps), steps, pay, 0, 0 }
#define MDEF_AFTER(title, giver, pos, intro, steps, pay, after) { title, giver, pos, intro, NPAGES(intro), NSTEPS(steps), steps, pay, after, NPAGES(after) }

/* =========================================================== ACT TWO: PAPER (Downtown) */
static const Page p10[] = {
    { "DEE", "NICK MELNYK? DEE OKAFOR, HARBOR", "GAZETTE. I WROTE ABOUT MORAN." },
    { "DEE", "MORAN'S DEAD, BUT THE EVICTIONS", "DIDN'T STOP. NOW THEY'RE LEGAL." },
    { "NICK", "PRYCE.", 0 },
    { "DEE", "I NEED PROOF. RIGHT NOW I NEED A", "RIDE. TWO CARS HAVE BEEN ON ME." },
    { "DEE", "PICK ME UP AT THE CAFE DOWNTOWN,", "ACROSS THE RIVER." } };
static const Step m10[] = {
    S_PICKUP(PED_DEE, NS(11, 3), 0, "PICK UP DEE DOWNTOWN", "ACROSS THE RIVER"),
    S_HUNT(2, F_LOSE, CAR_SEDAN_SILVER, "SHAKE THE TWO CARS", "WRECK THEM OR LOSE THEM"),
    S_GO(F_CAR, ROADN(13, 7), 0, "TAKE DEE TO THE GAZETTE", 0) };

static const Page p11[] = {
    { "BOHDAN", "THEY LET ME OUT. TWO CASTS, ONE", "BAD TEMPER. THANK YOU, NICK." },
    { "BOHDAN", "MRS. KOWALSKI ON PIER STREET. MEN", "ARE CARRYING HER THINGS OUT." },
    { "BOHDAN", "THEY HAVE PAPERS NOW. PRYCE'S", "LAWYERS SIGN, MORAN'S GOONS LIFT." },
    { "NICK", "PAPERS DON'T STOP BULLETS.", 0 },
    { "BOHDAN", "GO. BEFORE THE TRUCK LEAVES.", 0 } };
static const Step m11[] = {
    S_GO(F_CAR, ROADN(4, 7), 70, "GET TO PIER STREET", "BEFORE THE TRUCK LEAVES"),
    S_KILL(4, F_HERE, PED_SUIT, NS(4, 7), 100, "DRIVE OFF THE HIRED MUSCLE", 0),
    S_WRECK(1, CAR_BOXTRUCK_WHITE, KERB(4, 7), 0, 0, "WRECK THE EVICTION TRUCK", 0),
    S_HEAT(1, "LOSE THE COPS", 0) };

static const Page p12[] = {
    { "KANE", "TOMAS KANE. THE KINGS. YOU BURNED", "THREE OF MY CARS FOR MORAN." },
    { "NICK", "MORAN'S DEAD.", 0 },
    { "KANE", "MY CARS ARE STILL BURNED. HIS", "NEPHEW DANNY RUNS HIS CREW NOW." },
    { "KANE", "DANNY HAS A CAR LOT IN THE PORT.", "BRING ME THREE. THEN WE'RE EVEN." } };
static const Step m12[] = {
    S_CAR(CAR_MUSCLE_SILVER, KERB(11, 11), 1, "STEAL A CAR FROM DANNY'S LOT", "IN THE INDUSTRIAL PORT"),
    S_DROP(0, KERB(3, 12), 0, "BRING IT TO THE KINGS", "IN THE HEIGHTS"),
    S_CAR(CAR_TUNER_GREEN, KERB(11, 12), 1, "ONE DOWN. GO BACK FOR ANOTHER", 0),
    S_HEAT(2, "DANNY'S MEN CALLED THE COPS", "LOSE THEM"),
    S_DROP(0, KERB(3, 12), 0, "BRING IT TO THE KINGS", 0),
    S_CAR(CAR_SUPER_RED, KERB(12, 11), 1, "THE LAST ONE: DANNY'S OWN CAR", 0),
    S_HUNT(2, 0, CAR_SEDAN_BLUE, "DANNY'S CREW IS ON YOU", "WRECK THEM"),
    S_DROP(0, KERB(3, 12), 0, "BRING IT TO THE KINGS", 0) };

static const Page p13[] = {
    { "DEE", "PRYCE'S AIDE, LEON BAXTER, LEAVES", "CITY HALL AT NOON. EVERY DAY." },
    { "DEE", "TODAY HE ISN'T GOING TO LUNCH.", "FOLLOW HIM. DON'T GET CLOSE." },
    { "NICK", "AND IF HE SEES ME?", 0 },
    { "DEE", "THEN WE NEVER LEARN WHO HE MEETS.", "STAY BACK. STAY QUIET." } };
static const Step m13[] = {
    S_TAIL(CAR_LUXURY_BLACK, AT(14, 2, 0, 0), 75, "TAIL BAXTER'S BLACK CAR", "STAY BACK, DON'T LOSE HIM"),
    S_SPOT(DYN, "GET OUT AND LISTEN IN", "HE'S MEETING HALVORSEN SHIPPING"),
    S_SAY(0),
    S_GO(F_CAR | F_LOSE, ROADN(13, 7), 0, "BRING IT BACK TO DEE", "AT THE GAZETTE") };

static const Page p14[] = {
    { "DEE", "BAXTER MEETS HALVORSEN'S PEOPLE AT", "THE MARINA TONIGHT. I NEED PHOTOS." },
    { "DEE", "THREE ANGLES: ROOFTOP, DOCK AND", "THE PARKING LOT. TAKE MY CAMERA." },
    { "NICK", "I WAS A SOLDIER, NOT A PAPARAZZO.", 0 },
    { "DEE", "SOLDIERS KNOW HOW TO HIDE. GO.", 0 } };
static const Step m14[] = {
    S_GO(F_CAR, ROADN(18, 6), 0, "DRIVE TO THE MARINA", "ON THE EAST SHORE"),
    S_SPOT(NS(18, 5), "PHOTO FROM THE ROOF STAIRS", "CLICK."),
    S_SPOT(ES(19, 6), "NOW THE DOCK", "CLICK."),
    S_SPOT(SS(18, 7), "AND THE PARKING LOT", "CLICK. GOT THEM ALL"),
    S_KILL(3, F_HERE, PED_COP, SS(18, 7), 100, "BLACKLINE SECURITY SPOTTED YOU!", 0),
    S_HEAT(2, "LOSE THE COPS", 0) };

static const Page p15[] = {
    { "MARTA", "DANNY MORAN IS BURNING GARAGES", "THAT WON'T PAY HIM. OURS IS NEXT." },
    { "MARTA", "BOHDAN'S IN THERE WITH A WRENCH", "AND TWO BROKEN ARMS. HELP HIM." },
    { "NICK", "HOW MANY?", 0 },
    { "MARTA", "ENOUGH. THEN GO GET DANNY.", 0 } };
static const Step m15[] = {
    S_GO(F_CAR, SHOPPOS, 0, "GET TO BOHDAN'S AUTO", 0),
    S_DEFEND(3, PED_GREY, SHOPPOS, "DEFEND THE SHOP", "THREE WAVES"),
    S_CHASE(0, CAR_SUV_BLACK, AT(3, 7, 0, 0), 260, "DANNY'S RUNNING!", "WRECK HIS SUV") };

static const Page p16[] = {
    { "KANE", "DANNY TOOK ONE OF MY DRIVERS.", "LITTLE LUIS. SEVENTEEN YEARS OLD." },
    { "KANE", "THEY'RE MOVING HIM IN A WHITE VAN", "THROUGH THE PORT. RIGHT NOW." },
    { "NICK", "THEN WE DON'T SHOOT THE VAN.", 0 },
    { "KANE", "STAY ON IT TILL THEY STOP. THEN", "BRING LUIS HOME." } };
static const Step m16[] = {
    S_CHASE(F_BAIL, CAR_VAN_WHITE, AT(12, 12, 0, 0), 260, "CATCH THE WHITE VAN", "RIDE ITS BUMPER TILL IT STOPS"),
    S_KILL(2, F_HERE, PED_GREY, DYN, 100, "TAKE OUT THE KIDNAPPERS", 0),
    S_PICKUP(PED_GREEN, DYN, 0, "GET LUIS IN YOUR CAR", 0),
    S_GO(F_CAR | F_LOSE, KERB(3, 12), 0, "TAKE LUIS HOME TO THE KINGS", 0) };

static const Page p17[] = {
    { "JINX", "NICK! LONG TIME. OKAY, ONE WEEK.", "I HEAR THINGS. I SELL THINGS." },
    { "JINX", "PRYCE'S OFFICE IS SHREDDING FILES", "TONIGHT. A TRUCK TAKES THE BAGS." },
    { "JINX", "TO THE INCINERATOR IN THE PORT.", "GET IT FIRST. FREE TIP." },
    { "NICK", "NOTHING YOU SAY IS FREE.", 0 },
    { "JINX", "TEN PERCENT OF FREE. FOR FRIENDS.", 0 } };
static const Step m17[] = {
    S_CHASE(F_BAIL, CAR_BOXTRUCK_WHITE, AT(13, 3, 0, 0), 260, "STOP THE SHREDDING TRUCK", "RIDE ITS BUMPER"),
    S_KEEPCAR("GET IN THE TRUCK", 0),
    S_HEAT(2, "THE COPS ARE COMING", "LOSE THEM IN THE TRUCK"),
    S_DROP(0, ROADN(13, 7), 0, "TAKE THE BAGS TO THE GAZETTE", 0) };

static const Page p18[] = {
    { "DEE", "THE SHREDDER BAGS GAVE US NAMES.", "AND ONE BRIEFCASE WE DON'T HAVE." },
    { "DEE", "BAXTER CARRIES IT IN PRYCE'S", "MOTORCADE. SECOND CAR. TODAY." },
    { "DEE", "NOBODY TOUCHES THE COUNCILMAN.", "A DEAD SCANDAL DOESN'T SELL." },
    { "NICK", "I'LL BE GENTLE.", 0 } };
static const Step m18[] = {
    S_CHASE(F_BAIL, CAR_LUXURY_BLACK, AT(10, 4, 0, 0), 260, "CATCH THE SECOND CAR", "RIDE IT TILL BAXTER BAILS"),
    S_KILL(1, F_FLEE, PED_SUIT, DYN, 80, "BAXTER'S RUNNING WITH THE CASE", "STOP HIM"),
    S_GO(F_FOOT, DYN, 0, "GRAB THE BRIEFCASE", 0),
    S_HEAT(3, "LOSE THE HEAT", 0),
    S_GO(F_CAR, ROADN(13, 7), 0, "BRING THE CASE TO DEE", 0) };

static const Page p19[] = {
    { "KANE", "DANNY'S HOLED UP IN A WAREHOUSE", "IN THE PORT. MY BOYS ARE READY." },
    { "KANE", "WE GO IN THE FRONT. YOU GO IN", "WHEREVER YOU LIKE. JUST GET DANNY." },
    { "NICK", "AND THEN WE'RE EVEN.", 0 },
    { "KANE", "THEN WE'RE FRIENDS. WORSE DEAL.", 0 } };
static const Step m19[] = {
    S_GIVE(2, 240),
    S_GO(F_CAR, ROADN(12, 15), 0, "MEET THE KINGS AT THE WAREHOUSE", "IN THE PORT"),
    S_KILL(5, 0, PED_GREY, NS(12, 15), 110, "CLEAR OUT DANNY'S CREW", 0),
    S_CHASE(0, CAR_SUPER_RED, AT(12, 16, 0, 0), 260, "DANNY'S MAKING A RUN FOR IT", "WRECK HIS CAR"),
    S_KILL(1, F_FLEE, PED_SUIT, DYN, 120, "FINISH DANNY", 0) };

static const Page p20[] = {
    { "DEE", "WE RAN IT. FRONT PAGE. PRYCE,", "BAXTER, HALVORSEN. ALL OF IT." },
    { "DEE", "PRYCE SUED BY LUNCHTIME. NOW SOME", "MEN ARE AT THE GAZETTE WITH GAS." },
    { "NICK", "STAY AWAY FROM THE BUILDING.", 0 },
    { "DEE", "I'M INSIDE THE BUILDING, NICK.", 0 } };
static const Step m20[] = {
    S_GO(F_CAR, ROADN(13, 7), 80, "GET TO THE GAZETTE, FAST", 0),
    S_KILL(4, F_HERE, PED_SUIT, NS(13, 7), 100, "STOP THE ARSONISTS", 0),
    S_PICKUP(PED_DEE, NS(13, 7), 0, "GET DEE OUT", 0),
    S_SURVIVE(3, 45, "HALF THE CITY IS AFTER YOU", "STAY ALIVE"),
    S_HEAT(3, "NOW LOSE THEM", 0),
    S_GO(F_CAR, SAFEPOS, 0, "TAKE DEE TO THE SAFEHOUSE", "IN THE HEIGHTS"),
    S_SAY(1) };

/* =========================================================== ACT THREE: IRON (the Industrial port and the Heights) */
static const Page p21[] = {
    { "RUDY", "HALVORSEN'S PRIVATE ARMY CAME IN", "OVERNIGHT. BLACKLINE, THEY CALL IT." },
    { "RUDY", "CHECKPOINTS ALL OVER THE PORT.", "I NEED TO KNOW WHERE, HOW MANY." },
    { "NICK", "RECON. I'VE DONE THAT BEFORE.", 0 },
    { "RUDY", "ON FOOT. QUIET. FOUR SPOTS.", 0 } };
static const Step m21[] = {
    S_SPOT(NS(10, 11), "SCOUT THE FIRST CHECKPOINT", "ONE"),
    S_SPOT(ES(13, 12), "THE SECOND CHECKPOINT", "TWO"),
    S_SPOT(NS(16, 11), "THE THIRD CHECKPOINT", "THREE"),
    S_KILL(3, F_HERE, PED_COP, NS(16, 11), 110, "THEY SAW YOU!", "DEAL WITH THEM"),
    S_SPOT(SS(11, 17), "THE LAST CHECKPOINT", "FOUR"),
    S_GO(F_LOSE, NS(7, 6), 0, "REPORT BACK TO RUDY", "AT THE DOCKS") };

static const Page p22[] = {
    { "MARTA", "A HALVORSEN TRUCK LEAVES THE PORT", "TONIGHT. ONE SEALED CONTAINER." },
    { "MARTA", "JINX SAYS IT'S PAPERWORK. NOBODY", "GUARDS PAPERWORK WITH RIFLES." },
    { "NICK", "BRING IT TO THE SHOP?", 0 },
    { "MARTA", "IN ONE PIECE. I WANT TO OPEN IT.", 0 } };
static const Step m22[] = {
    S_CHASE(F_BAIL, CAR_BOXTRUCK_WHITE, AT(17, 13, 0, 0), 260, "CATCH THE HALVORSEN TRUCK", "RIDE ITS BUMPER"),
    S_KEEPCAR("TAKE THE TRUCK", 0),
    S_HUNT(2, F_LOSE, CAR_SUV_BLACK, "BLACKLINE IS AFTER THE TRUCK", "LOSE THEM OR WRECK THEM"),
    S_DROP(0, SHOPPOS, 0, "BRING IT TO BOHDAN'S AUTO", 0),
    S_SAY(2) };

static const Page p23[] = {
    { "PRYCE", "MR. MELNYK. EDGAR PRYCE. PLEASE,", "SIT. THE VIEW IS THE BEST PART." },
    { "PRYCE", "YOUR UNCLE'S DEED. SIGNED, CLEAN.", "YOURS IF YOU GO HOME TO UKRAINE." },
    { "NICK", "MY UNCLE IS HOME. SO AM I.", 0 },
    { "PRYCE", "PITY. MS. HALVORSEN'S PEOPLE WILL", "SEE YOU OUT." } };
static const Step m23[] = {
    S_KILL(4, F_HERE, PED_COP, SS(6, 17), 110, "BLACKLINE!", "GET OFF THE GROUNDS"),
    S_HUNT(2, F_LOSE, CAR_SUV_BLACK, "SHAKE THE SUVS", 0),
    S_GO(F_CAR | F_LOSE, SAFEPOS, 0, "GET BACK TO THE SAFEHOUSE", 0) };

static const Page p24[] = {
    { "BOHDAN", "A CAR SITS OUTSIDE THE SHOP ALL", "NIGHT. THE SAME CAR FOLLOWS ME." },
    { "BOHDAN", "I AM AN OLD MAN IN CASTS. I DON'T", "LIKE TO BE FOLLOWED." },
    { "NICK", "I'LL DRIVE YOU HOME TONIGHT.", 0 },
    { "BOHDAN", "GOOD. AND THEN YOU TALK TO THEM.", 0 } };
static const Step m24[] = {
    S_PICKUP(PED_BOHDAN, AT(1, 6, 13, 30), 0, "PICK UP BOHDAN AT THE SHOP", 0),
    S_HUNT(2, 0, CAR_SEDAN_SILVER, "THEY'RE FOLLOWING YOU", "DEAL WITH THEM"),
    S_GO(F_CAR, ROADN(2, 15), 0, "DRIVE BOHDAN HOME", "IN THE HEIGHTS"),
    S_KILL(3, F_HERE, PED_SUIT, NS(2, 15), 100, "MORE WAITING AT THE APARTMENT!", 0) };

static const Page p25[] = {
    { "KANE", "BLACKLINE KEEPS ITS ARMORED SUVS", "AT THE SCRAPYARD IN THE PORT." },
    { "KANE", "TOMORROW THEY ROLL INTO MY STREETS.", "TONIGHT THEY BURN." },
    { "KANE", "TAKE THIS. A GIFT FROM THE KINGS.", 0 },
    { "NICK", "A ROCKET LAUNCHER. SUBTLE.", 0 } };
static const Step m25[] = {
    S_GIVE(5, 6),
    S_GO(F_CAR, ROADN(17, 12), 0, "GO TO THE SCRAPYARD", "IN THE PORT"),
    S_WRECK(3, CAR_SUV_BLACK, KERB(17, 12), 4, 1, "DESTROY BLACKLINE'S SUVS", "(L: SWITCH WEAPON)"),
    S_HEAT(3, "LOSE THE COPS", 0) };

static const Page p26[] = {
    { "DEE", "THE FORGED DEEDS WERE PRINTED IN", "HALVORSEN'S TOWER. ON ONE DRIVE." },
    { "DEE", "A COURIER TAKES BACKUPS ACROSS", "TOWN EVERY MORNING. SMALL RED CAR." },
    { "NICK", "AND THE COURIER?", 0 },
    { "DEE", "IS A KID WITH A JOB. SCARE HIM.", "BRING ME THE DRIVE." } };
static const Step m26[] = {
    S_CHASE(F_BAIL, CAR_HATCH5_RED, AT(15, 5, 0, 0), 260, "CATCH THE RED COURIER CAR", "RIDE ITS BUMPER"),
    S_GO(F_FOOT, DYN, 0, "THE KID RAN. GET THE DRIVE", "FROM THE CAR"),
    S_HUNT(2, 0, CAR_SUV_BLACK, "BLACKLINE WANTS THAT DRIVE", "WRECK THEM"),
    S_GO(F_CAR, ROADN(10, 1), 0, "GET THE DRIVE TO DEE", "AT HER NEW OFFICE") };

static const Page p27[] = {
    { "JINX", "HALVORSEN KEEPS THE REAL DEEDS IN", "A SAFE AT HER HOUSE UP NORTH." },
    { "JINX", "SORRY, SOUTH. IN THE HEIGHTS. I'M", "BAD WITH MAPS. GOOD WITH SAFES." },
    { "JINX", "YOU DRIVE, I CRACK. FIFTY-FIFTY.", 0 },
    { "NICK", "WHY DO I FEEL LIKE THE FIFTY?", 0 } };
static const Step m27[] = {
    S_CAR(CAR_TUNER_GREEN, KERB(4, 13), 1, "GET IN THE GETAWAY CAR", 0),
    S_GO(F_MCAR, ROADN(5, 18), 0, "DRIVE TO HALVORSEN'S HOUSE", 0),
    S_SAY(3),
    S_KILL(5, F_HERE, PED_COP, NS(5, 18), 110, "IT'S A TRAP!", "FIGHT YOUR WAY OUT"),
    S_HEAT(3, "LOSE THE HEAT", 0),
    S_GO(F_CAR, SHOPPOS, 0, "GET BACK TO BOHDAN'S AUTO", 0) };

static const Page p28[] = {
    { "MARTA", "JINX SOLD YOU OUT. BLACKLINE KNOWS", "WHERE THE SAFEHOUSE IS." },
    { "MARTA", "THEY'RE OUTSIDE RIGHT NOW. AND", "THEY BROUGHT THE POLICE." },
    { "NICK", "THEN WE LEAVE LOUDLY.", 0 } };
static const Step m28[] = {
    S_KILL(5, F_HERE, PED_COP, NS(5, 12), 110, "FIGHT YOUR WAY OUT", 0),
    S_SURVIVE(4, 40, "FOUR STARS", "STAY ALIVE"),
    S_HEAT(4, "NOW LOSE THEM", 0),
    S_GO(F_CAR, WS(0, 3), 0, "GET TO MARTA'S BACKUP GARAGE", "IN THE DOCKS") };

static const Page p29[] = {
    { "MARTA", "COLE MERCER HAS RUDY. BLACKLINE'S", "FIELD BOSS. EX-ARMY, LIKE YOU." },
    { "MARTA", "THEY'RE HOLDING HIM AT A COMPOUND", "IN THE SOUTH OF THE HEIGHTS." },
    { "NICK", "LIKE ME? WE'LL SEE.", 0 } };
static const Step m29[] = {
    S_GIVE(4, 60),
    S_GO(F_CAR, ROADN(2, 18), 0, "DRIVE TO THE BLACKLINE COMPOUND", 0),
    S_KILL(6, 0, PED_COP, NS(2, 18), 120, "CLEAR THE COMPOUND", 0),
    S_PICKUP(PED_WHITE, SS(2, 18), 0, "GET RUDY OUT", 0),
    S_HUNT(2, F_LOSE, CAR_SUV_BLACK, "MERCER'S SENDING EVERYTHING", "LOSE THEM OR WRECK THEM"),
    S_GO(F_CAR, NS(7, 6), 0, "TAKE RUDY TO THE DOCKS", 0) };

static const Page p30[] = {
    { "JINX", "DON'T SHOOT. PLEASE. THEY BEAT ME", "AND DUMPED ME HERE. I DESERVE IT." },
    { "JINX", "THEY HAD MY MOTHER, NICK. THEY LET", "HER GO WHEN I GAVE THEM YOU." },
    { "JINX", "PRYCE AND HALVORSEN MEET ON GULL", "ISLAND WHEN THE CASINO GOES UP." },
    { "JINX", "THAT'S EVERYTHING. DO WHAT YOU", "HAVE TO." } };
static const Step m30[] = {
    S_CHOICE(1, PED_GREEN, AT(4, 8, 44, 13), "SHOOT JINX, OR WALK UP", "TO HIM AND LET HIM GO") };

static const Page p31[] = {
    { "KANE", "PRYCE IS CLOSING EVERY ROAD TO", "GULL ISLAND. THE BRIDGE GOES UP." },
    { "KANE", "MARTA'S CREW NEEDS TIME TO CROSS.", "HOLD THE CONTROL HOUSE." },
    { "NICK", "HOW LONG?", 0 },
    { "KANE", "DON'T LOOK AT YOUR WATCH.", 0 } };
static const Page a31[] = {
    { "KANE", "THEY'RE ACROSS. THE ISLAND'S OPEN.", 0 },
    { "KANE", "WHATEVER HAPPENS OVER THERE, NICK,", "THE KINGS OWE YOU ONE." } };
static const Step m31[] = {
    S_GO(F_CAR, AT(15, 14, 40, 6), 0, "GET TO THE BRIDGE CONTROL HOUSE", "NORTH OF GULL ISLAND"),
    S_DEFEND(4, PED_COP, AT(15, 14, 40, 13), "HOLD THE CONTROL HOUSE", "FOUR WAVES") };

/* =========================================================== ACT FOUR: TIDE (Gull Island) */
static const Page p32[] = {
    { "RUDY", "TONIGHT WE GO TO THE ISLAND. ALL", "OF US. YOU FIRST." },
    { "RUDY", "BLACKLINE WATCHES THE NORTH QUAY.", "CHECK THEIR POSTS, QUIETLY." },
    { "RUDY", "SAL WOULD HAVE LIKED THIS. HE", "HATED PEOPLE WHO BUILD CASINOS." } };
static const Step m32[] = {
    S_GO(F_CAR, ROADN(16, 15), 0, "CROSS TO GULL ISLAND", 0),
    S_SPOT(NS(17, 15), "CHECK THE FIRST POST", "CLEAR"),
    S_SPOT(ES(18, 15), "CHECK THE SECOND POST", "CLEAR"),
    S_KILL(3, 0, PED_COP, NS(17, 16), 110, "TAKE OUT THE PATROL", 0),
    S_GO(F_FOOT, NS(17, 16), 0, "GET TO THE CONSTRUCTION SITE", 0) };

static const Page p33[] = {
    { "MARTA", "THREE GENERATOR TRUCKS POWER THE", "CAMERAS. KILL THEM, THEY GO BLIND." },
    { "NICK", "AND THE GUARDS?", 0 },
    { "MARTA", "WILL BE VERY CONFUSED.", 0 } };
static const Step m33[] = {
    S_WRECK(3, CAR_BOXTRUCK_WHITE, KERB(16, 17), 4, 1, "DESTROY THE GENERATOR TRUCKS", 0),
    S_KILL(3, F_HERE, PED_COP, NS(17, 17), 110, "MOP UP THE GUARDS", 0) };

static const Page p34[] = {
    { "KANE", "MY BOYS HIT THE FRONT GATE AT", "MIDNIGHT. YOU TAKE THE BACK." },
    { "KANE", "HOLD IT. NOBODY GETS OUT THAT WAY.", 0 } };
static const Step m34[] = {
    S_GO(F_CAR, ROADN(19, 17), 0, "GET TO THE BACK GATE", 0),
    S_DEFEND(3, PED_COP, NS(19, 17), "HOLD THE BACK GATE", 0),
    S_KILL(5, 0, PED_COP, NS(18, 17), 120, "CLEAR THE YARD", 0) };

static const Page p35[] = {
    { "DEE", "THE GAZETTE IS GONE, BUT I HAVE A", "CAMERA, A SIGNAL AND THE DEEDS." },
    { "DEE", "GET ME TO THE TOP OF THE CRANE.", "WE GO LIVE TO THE WHOLE CITY." },
    { "NICK", "AND IF THEY SHOOT AT THE CRANE?", 0 },
    { "DEE", "THEN IT'S A GREAT BROADCAST.", 0 } };
static const Step m35[] = {
    S_PICKUP(PED_DEE, AT(15, 15, 44, 13), 0, "PICK UP DEE", 0),
    S_HUNT(2, 0, CAR_SUV_BLACK, "BLACKLINE IS COMING FOR DEE", "WRECK THEM"),
    S_GO(F_CAR, ROADN(18, 16), 0, "DRIVE DEE TO THE CRANE", 0),
    S_DEFEND(3, PED_COP, NS(18, 16), "PROTECT DEE WHILE SHE BROADCASTS", 0),
    S_SAY(4) };

static const Page p36[] = {
    { "MERCER", "SERGEANT MELNYK. I READ YOUR FILE.", "GOOD SOLDIER. WRONG SIDE." },
    { "NICK", "I DON'T FIGHT FOR CASINOS.", 0 },
    { "MERCER", "EVERYONE FIGHTS FOR SOMEBODY.", 0 } };
static const Step m36[] = {
    S_GIVE(4, 60),
    S_BOSS(PED_COP, 3, 1, NS(17, 18), 900, "TAKE DOWN COLE MERCER", "HE'S ARMORED. KEEP MOVING") };

static const Page p37[] = {
    { "RUDY", "HALVORSEN'S RUNNING. SHE HAS A CAR", "WAITING AND A JET AT THE PORT." },
    { "RUDY", "BIG MAROON BOAT OF A CAR. YOU", "CAN'T MISS IT. DON'T LET HER GO." } };
static const Page a37[] = {
    { "RUDY", "SHE'S IN CUFFS. DEE'S CAMERA GOT", "EVERY SECOND OF IT." },
    { "RUDY", "ONE LEFT, NICK. THE COUNCILMAN.", 0 } };
static const Step m37[] = {
    S_CHASE(0, CAR_YACHT_MAROON, AT(18, 18, 0, 0), 280, "STOP HALVORSEN'S CAR", "WRECK IT"),
    S_KILL(3, F_HERE, PED_COP, DYN, 110, "HER BODYGUARDS ARE OUT", 0) };

static const Page p38[] = {
    { "DEE", "PRYCE IS IN THE CASINO TOWER.", "HIS HELICOPTER IS ON THE ROOF." },
    { "DEE", "GUARDS ON EVERY FLOOR. GO.", 0 },
    { "NICK", "SAVE ME A LINE IN THE STORY.", 0 } };
static const Step m38[] = {
    S_KILL(4, 0, PED_SUIT, NS(18, 19), 120, "FIGHT THROUGH PRYCE'S GUARDS", 0),
    S_KILL(4, F_HERE, PED_COP, ES(18, 19), 130, "MORE ON THE NEXT FLOOR", 0),
    S_GO(F_FOOT, ES(19, 19), 0, "GET TO THE HELIPAD", 0) };

static const Page p39[] = {
    { "PRYCE", "YOU THINK THIS WAS ABOUT HOUSES?", "IT'S ABOUT WHO OWNS THE WATER." },
    { "PRYCE", "SAL WOULDN'T SELL HIS PIER. NOR", "HIS TUG. SO WE TOOK BOTH." },
    { "NICK", "YOU SANK THE LUCKY MARIE.", 0 },
    { "PRYCE", "MORAN DID. I ONLY SIGNED THINGS.", "DEE'S CAMERA IS ON, ISN'T IT?" } };
static const Page a39[] = {        /* Pryce shot */
    { "", "PRYCE GOES DOWN WITH THE CAMERA", "STILL RUNNING." },
    { "DEE", "THE WHOLE CITY SAW IT, NICK. THEY", "WILL CALL YOU A HERO. OR WORSE." },
    { "NICK", "SAL CAN REST. THAT'S ENOUGH.", 0 } };
static const Page st_end_arrest[] = {   /* Pryce taken alive */
    { "", "SIRENS. THE POLICE TAKE PRYCE", "AWAY IN CUFFS, LIVE ON CAMERA." },
    { "DEE", "THE STORY WINS. HE'LL GROW OLD IN", "A CELL. SLOWER. BETTER." },
    { "NICK", "SAL CAN REST NOW.", 0 } };
static const Step m39[] = {
    S_CHOICE(2, PED_SUIT, AT(19, 18, 40, 9), "SHOOT PRYCE, OR WALK UP TO HIM", "AND LET THE POLICE TAKE HIM") };

static const Page p40[] = {
    { "BOHDAN", "IT'S OVER, NICK. THE DEEDS ARE", "VOID. THE WATERFRONT STAYS OURS." },
    { "BOHDAN", "DRIVE ME HOME. MY ARMS ARE BETTER,", "BUT I STILL CAN'T WORK A GEARBOX." } };
static const Page a40[] = {
    { "BOHDAN", "YOUR FATHER WOULD BE PROUD, NICK.", "STAY. THE SHOP NEEDS HANDS." },
    { "NICK", "I'LL STAY. SOMEONE HAS TO FIX THE", "CARS I KEEP BREAKING." },
    { "", "THE END. THANKS FOR PLAYING.", "THE CITY IS YOURS. FREE ROAM ON." } };
static const Step m40[] = {
    S_PICKUP(PED_BOHDAN, AT(15, 15, 44, 13), 0, "PICK UP BOHDAN", 0),
    S_GO(F_CAR, SHOPPOS, 0, "DRIVE BOHDAN HOME TO THE SHOP", 0) };

/* mid-mission phone calls (S_SAY) */
static const Page say0[] = {
    { "DEE", "HALVORSEN SHIPPING. SO PRYCE HAS A", "PARTNER WITH SHIPS AND GUNS." },
    { "DEE", "COME BACK. CAREFULLY.", 0 } };
static const Page say1[] = {
    { "DEE", "PRYCE'S LAWYERS GOT THE FILES", "THROWN OUT. THE STORY IS DEAD." },
    { "DEE", "WE PROVED IT AND IT DIDN'T MATTER.", 0 },
    { "NICK", "THEN WE STOP PLAYING BY HIS RULES.", 0 } };
static const Page say2[] = {
    { "MARTA", "DEEDS. TWO HUNDRED DOCK HOMES, ALL", "FORGED. BOHDAN'S SHOP IS ON TOP." },
    { "MARTA", "HALVORSEN PRINTS THEM, PRYCE SIGNS.", 0 } };
static const Page say3[] = {
    { "JINX", "NICK... I'M SORRY. I'M SO SORRY.", 0 },
    { "JINX", "IT'S A TRAP. RUN!", 0 } };
static const Page say4[] = {
    { "DEE", "THIS IS DEE OKAFOR, LIVE FROM GULL", "ISLAND. THESE ARE FORGED DEEDS." },
    { "DEE", "SIGNED BY COUNCILMAN EDGAR PRYCE.", "TWO HUNDRED FAMILIES. WATCH." } };
static const struct { const Page *pg; u8 n; } story_says[] = {
    { say0, NPAGES(say0) }, { say1, NPAGES(say1) }, { say2, NPAGES(say2) }, { say3, NPAGES(say3) }, { say4, NPAGES(say4) } };

static const MissionDef story_defs[NMISSIONS - NACT1] = {
    MDEF("FRONT PAGE", PED_DEE, NS(7, 4), p10, m10, 4000),
    MDEF("EVICTION NOTICE", PED_BOHDAN, NS(1, 5), p11, m11, 4500),
    MDEF("CROWN DEBT", PED_SUIT, NS(3, 11), p12, m12, 5000),
    MDEF("CITY HALL INK", PED_DEE, NS(12, 5), p13, m13, 5000),
    MDEF("PHOTO OP", PED_DEE, NS(13, 6), p14, m14, 5500),
    MDEF("DANNY BOY", PED_MARTA, NS(2, 6), p15, m15, 6000),
    MDEF("KINGS' RANSOM", PED_SUIT, NS(3, 12), p16, m16, 6500),
    MDEF("SHREDDER", PED_GREEN, NS(11, 5), p17, m17, 7000),
    MDEF("MOTORCADE", PED_DEE, ES(12, 6), p18, m18, 7500),
    MDEF("MORAN'S HEIR", PED_SUIT, NS(3, 12), p19, m19, 8000),
    MDEF("RETRACTION", PED_DEE, NS(11, 7), p20, m20, 9000),
    MDEF("NEW MANAGEMENT", PED_WHITE, NS(7, 6), p21, m21, 7000),
    MDEF("CONTAINER 7", PED_MARTA, NS(2, 6), p22, m22, 8500),
    MDEF("THE OFFER", PED_SUIT, SS(6, 17), p23, m23, 6000),
    MDEF("NIGHT SHIFT", PED_BOHDAN, NS(1, 5), p24, m24, 7000),
    MDEF("IRON YARD", PED_SUIT, NS(3, 12), p25, m25, 8000),
    MDEF("PAPER TRAIL", PED_DEE, NS(10, 1), p26, m26, 8000),
    MDEF("HILL CLIMB", PED_GREEN, NS(4, 13), p27, m27, 9000),
    MDEF("BURNED", PED_MARTA, NS(5, 12), p28, m28, 9000),
    MDEF("MERCER", PED_MARTA, NS(0, 4), p29, m29, 10000),
    MDEF("WHAT JINX KNEW", PED_GREEN, NS(4, 8), p30, m30, 3000),
    MDEF_AFTER("DRAWBRIDGE", PED_SUIT, NS(13, 13), p31, m31, 10000, a31),
    MDEF("LANDING PARTY", PED_WHITE, NS(7, 6), p32, m32, 9000),
    MDEF("POWER DOWN", PED_MARTA, NS(16, 15), p33, m33, 10000),
    MDEF("HARD HATS", PED_SUIT, NS(15, 16), p34, m34, 10000),
    MDEF("LIVE FEED", PED_DEE, NS(15, 15), p35, m35, 11000),
    MDEF("MERCER, AGAIN", PED_COP, NS(16, 18), p36, m36, 12000),
    MDEF_AFTER("HALVORSEN SAILS", PED_WHITE, NS(16, 17), p37, m37, 12000, a37),
    MDEF("COUNCILMAN", PED_DEE, NS(17, 19), p38, m38, 12000),
    MDEF_AFTER("THE LUCKY MARIE", PED_SUIT, NS(19, 18), p39, m39, 15000, a39),
    MDEF_AFTER("HOME", PED_BOHDAN, NS(15, 16), p40, m40, 20000, a40) };
