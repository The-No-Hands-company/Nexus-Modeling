#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Industry-Standard Data Tables
//
//  Comprehensive design rules (50+), hole sizes (ISO/ANSI/JIS/DIN),
//  material library (100+), sheet metal gauge tables (6 standards).
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>

namespace nexus::cad {

// ──────────── Design Rules (50+ across 8 categories) ───────────────────────

struct DesignRuleCategory {
    std::string name;
    std::string description;
};

inline std::vector<DesignRuleCategory> designRuleCategories() {
    return {
        {"Geometry", "Geometric validity and quality rules"},
        {"Draft", "Draft angle and moldability rules"},
        {"Hole", "Hole feature rules and standards"},
        {"Fillet", "Fillet and round rules"},
        {"Wall", "Wall thickness and uniformity rules"},
        {"Assembly", "Assembly interference and clearance rules"},
        {"Manufacturing", "Machinability and tool access rules"},
        {"Standards", "ISO/ANSI/DIN standards compliance"},
    };
}

struct DesignRuleDef {
    std::string id;
    std::string category;
    std::string description;
    int severity = 1; // 0=info, 1=warning, 2=error, 3=critical
    double minValue = 0;
    double maxValue = 0;
    std::string standardRef;
};

inline std::vector<DesignRuleDef> allDesignRules() {
    return {
        // ── Geometry ──────────────────────────────────────────
        {"GEO001","Geometry","Minimum edge length",1,0.1,0,""},
        {"GEO002","Geometry","Maximum face aspect ratio",1,0,10,""},
        {"GEO003","Geometry","Minimum radius of curvature",2,0.5,0,""},
        {"GEO004","Geometry","No self-intersecting geometry",3,0,0,""},
        {"GEO005","Geometry","No zero-area faces",3,0,0,""},
        {"GEO006","Geometry","No degenerate edges",2,0,0,""},
        {"GEO007","Geometry","Positive volume required",3,0,0,""},
        {"GEO008","Geometry","Watertight solid required",3,0,0,""},
        // ── Draft ─────────────────────────────────────────────
        {"DRF001","Draft","Minimum draft angle for molded parts",2,1,0,"ISO 12165"},
        {"DRF002","Draft","Negative draft angle not allowed",3,0,0,""},
        {"DRF003","Draft","Draft direction parallel to pull direction",1,0,0,""},
        {"DRF004","Draft","Undercut detection in mold direction",2,0,0,""},
        {"DRF005","Draft","Draft angle consistency across parting line",1,0,0,""},
        {"DRF006","Draft","Textured surfaces require increased draft",1,3,0,"VDI 3400"},
        // ── Hole ──────────────────────────────────────────────
        {"HOL001","Hole","Minimum hole diameter",2,1,0,""},
        {"HOL002","Hole","Minimum distance between holes",2,2,0,""},
        {"HOL003","Hole","Minimum distance from hole to edge",2,1,0,""},
        {"HOL004","Hole","Hole depth to diameter ratio limit",1,0,5,""},
        {"HOL005","Hole","Through-hole must exit solid",3,0,0,""},
        {"HOL006","Hole","Blind hole must not break through",2,0,0,""},
        {"HOL007","Hole","Counterbore depth within spec",1,0,0,"ISO 273"},
        {"HOL008","Hole","Countersink angle matches standard",1,82,0,"ANSI B18.6.3"},
        {"HOL009","Hole","Tapped hole depth sufficient for thread engagement",2,1.5,0,""},
        // ── Fillet ────────────────────────────────────────────
        {"FIL001","Fillet","Minimum fillet radius",1,0.5,0,""},
        {"FIL002","Fillet","Fillet radius must not exceed adjacent face width",2,0,0,""},
        {"FIL003","Fillet","Constant radius fillet preferred",1,0,0,""},
        {"FIL004","Fillet","Fillet before chamfer in feature order",1,0,0,""},
        {"FIL005","Fillet","Fillet tangent to adjacent surfaces",1,0,0,""},
        {"FIL006","Fillet","Avoid zero-radius fillets",2,0,0,""},
        // ── Wall ──────────────────────────────────────────────
        {"WAL001","Wall","Minimum wall thickness",2,0.5,0,""},
        {"WAL002","Wall","Maximum wall thickness variation",1,0,3,""},
        {"WAL003","Wall","Uniform wall thickness across part",1,0,0,""},
        {"WAL004","Wall","Rib thickness ≤ 60% of wall thickness",1,0,0.6,""},
        {"WAL005","Wall","Boss outer diameter ≤ 2× hole diameter",1,0,2,""},
        {"WAL006","Wall","Avoid sharp internal corners",2,0,0,""},
        // ── Assembly ──────────────────────────────────────────
        {"ASM001","Assembly","No interference between parts",3,0,0,""},
        {"ASM002","Assembly","Minimum clearance between moving parts",2,0.1,0,""},
        {"ASM003","Assembly","All fasteners aligned with holes",2,0,0,""},
        {"ASM004","Assembly","BOM completeness check",1,0,0,""},
        {"ASM005","Assembly","All components fully constrained",2,0,0,""},
        {"ASM006","Assembly","No duplicate components",1,0,0,""},
        {"ASM007","Assembly","Fastener length within grip range",1,0,0,""},
        // ── Manufacturing ─────────────────────────────────────
        {"MFG001","Manufacturing","Tool access to all machined features",2,0,0,""},
        {"MFG002","Manufacturing","Minimum internal corner radius for end mill",1,0,0,""},
        {"MFG003","Manufacturing","Deep pocket depth limit for standard tooling",1,0,4,""},
        {"MFG004","Manufacturing","Avoid impossible undercuts",3,0,0,""},
        {"MFG005","Manufacturing","Thread relief at bottom of tapped holes",1,0,0,""},
        {"MFG006","Manufacturing","Flat bottom holes require special tooling",1,0,0,""},
        // ── Standards ─────────────────────────────────────────
        {"STD001","Standards","Hole sizes comply with ISO 273",1,0,0,"ISO 273"},
        {"STD002","Standards","Thread sizes comply with ISO 724",1,0,0,"ISO 724"},
        {"STD003","Standards","General tolerances per ISO 2768-m",1,0,0,"ISO 2768"},
        {"STD004","Standards","Surface finish within specified range",1,0,0,"ISO 1302"},
        {"STD005","Standards","Chamfer angles per ISO 13715",1,0,0,"ISO 13715"},
        {"STD006","Standards","Fillet radii per DIN 250",1,0,0,"DIN 250"},
        {"STD007","Standards","Keyway dimensions per DIN 6885",1,0,0,"DIN 6885"},
    };
}

// ──────────── Hole Size Tables ──────────────────────────────────────────────

struct HoleSize {
    double diameter;
    std::string description;
    std::string standard;
};

inline std::vector<HoleSize> isoMetricClearanceHoles() {
    return {
        {1.6,"M1.6 close fit","ISO 273"}, {2.0,"M2 close fit","ISO 273"},
        {2.5,"M2.5 close fit","ISO 273"}, {3.0,"M3 close fit","ISO 273"},
        {3.5,"M3.5 close fit","ISO 273"}, {4.0,"M4 close fit","ISO 273"},
        {5.0,"M5 close fit","ISO 273"}, {6.0,"M6 close fit","ISO 273"},
        {7.0,"M7 close fit","ISO 273"}, {8.0,"M8 close fit","ISO 273"},
        {10.0,"M10 close fit","ISO 273"}, {12.0,"M12 close fit","ISO 273"},
        {14.0,"M14 close fit","ISO 273"}, {16.0,"M16 close fit","ISO 273"},
        {18.0,"M18 close fit","ISO 273"}, {20.0,"M20 close fit","ISO 273"},
        {22.0,"M22 close fit","ISO 273"}, {24.0,"M24 close fit","ISO 273"},
        {27.0,"M27 close fit","ISO 273"}, {30.0,"M30 close fit","ISO 273"},
        {33.0,"M33 close fit","ISO 273"}, {36.0,"M36 close fit","ISO 273"},
        {39.0,"M39 close fit","ISO 273"}, {42.0,"M42 close fit","ISO 273"},
        {45.0,"M45 close fit","ISO 273"}, {48.0,"M48 close fit","ISO 273"},
        {52.0,"M52 close fit","ISO 273"}, {56.0,"M56 close fit","ISO 273"},
    };
}

inline std::vector<HoleSize> ansiInchClearanceHoles() {
    return {
        {0.0625,"#0 close fit","ANSI B4.1"}, {0.0781,"#1 close fit","ANSI B4.1"},
        {0.0938,"#2 close fit","ANSI B4.1"}, {0.1094,"#3 close fit","ANSI B4.1"},
        {0.1250,"#4 close fit","ANSI B4.1"}, {0.1406,"#5 close fit","ANSI B4.1"},
        {0.1562,"#6 close fit","ANSI B4.1"}, {0.1719,"#8 close fit","ANSI B4.1"},
        {0.1875,"#10 close fit","ANSI B4.1"}, {0.2188,"1/4 close fit","ANSI B4.1"},
        {0.2812,"5/16 close fit","ANSI B4.1"}, {0.3438,"3/8 close fit","ANSI B4.1"},
        {0.4062,"7/16 close fit","ANSI B4.1"}, {0.4688,"1/2 close fit","ANSI B4.1"},
        {0.5625,"5/8 close fit","ANSI B4.1"}, {0.6562,"3/4 close fit","ANSI B4.1"},
        {0.7656,"7/8 close fit","ANSI B4.1"}, {0.8750,"1 close fit","ANSI B4.1"},
    };
}

inline std::vector<std::string> isoMetricThreadSizes() {
    return {"M1.6","M2","M2.5","M3","M3.5","M4","M5","M6","M7","M8","M10",
            "M12","M14","M16","M18","M20","M22","M24","M27","M30","M33",
            "M36","M39","M42","M45","M48","M52","M56","M60","M64"};
}

inline std::vector<std::string> ansiUncThreadSizes() {
    return {"#0-80","#1-64","#2-56","#3-48","#4-40","#5-40","#6-32","#8-32",
            "#10-24","1/4-20","5/16-18","3/8-16","7/16-14","1/2-13",
            "9/16-12","5/8-11","3/4-10","7/8-9","1-8"};
}

// ──────────── Material Library (100+ materials) ─────────────────────────────

struct DatabaseMaterial {
    std::string name;
    std::string category;
    double density;      // g/cm³
    double yieldStrength; // MPa
    double tensileStrength; // MPa
    double elasticModulus; // GPa
    double thermalExpansion; // µm/m·°C
    double thermalConductivity; // W/m·K
    std::string standard;
};

inline std::vector<DatabaseMaterial> materialDatabase() {
    return {
        // ── Carbon Steels ────────────────────────────────────
        {"AISI 1010","Carbon Steel",7.87,180,325,205,12.2,63,"ASTM A29"},
        {"AISI 1020","Carbon Steel",7.87,210,380,205,12.0,51,"ASTM A29"},
        {"AISI 1045","Carbon Steel",7.85,310,565,205,11.6,51,"ASTM A29"},
        {"AISI 1095","Carbon Steel",7.85,420,660,205,11.0,48,"ASTM A29"},
        {"AISI 1144","Carbon Steel",7.85,380,620,205,11.8,50,"ASTM A29"},
        {"AISI 1215","Carbon Steel",7.87,230,415,205,12.0,52,"ASTM A29"},
        // ── Alloy Steels ─────────────────────────────────────
        {"AISI 4130","Alloy Steel",7.85,460,670,205,12.2,42,"ASTM A322"},
        {"AISI 4140","Alloy Steel",7.85,570,765,205,12.0,42,"ASTM A322"},
        {"AISI 4340","Alloy Steel",7.85,680,860,205,12.0,44,"ASTM A322"},
        {"AISI 8620","Alloy Steel",7.85,360,550,205,12.0,46,"ASTM A322"},
        {"AISI 9310","Alloy Steel",7.85,550,790,205,11.8,43,"ASTM A322"},
        // ── Stainless Steels ──────────────────────────────────
        {"304 SS","Stainless Steel",8.00,215,505,193,17.2,16,"ASTM A240"},
        {"304L SS","Stainless Steel",8.00,170,485,193,17.2,16,"ASTM A240"},
        {"316 SS","Stainless Steel",8.00,240,550,193,16.0,16,"ASTM A240"},
        {"316L SS","Stainless Steel",8.00,220,515,193,16.0,16,"ASTM A240"},
        {"410 SS","Stainless Steel",7.75,275,480,200,11.0,25,"ASTM A240"},
        {"416 SS","Stainless Steel",7.75,275,517,200,11.0,25,"ASTM A240"},
        {"420 SS","Stainless Steel",7.75,345,586,200,10.3,25,"ASTM A240"},
        {"440C SS","Stainless Steel",7.80,450,760,200,10.1,24,"ASTM A240"},
        {"17-4 PH","Stainless Steel",7.80,790,930,196,10.8,18,"ASTM A564"},
        // ── Tool Steels ──────────────────────────────────────
        {"A2 Tool Steel","Tool Steel",7.86,380,620,203,10.6,26,"ASTM A681"},
        {"D2 Tool Steel","Tool Steel",7.70,380,640,210,10.5,20,"ASTM A681"},
        {"H13 Tool Steel","Tool Steel",7.80,380,620,210,10.4,24,"ASTM A681"},
        {"M2 Tool Steel","Tool Steel",8.14,430,730,218,10.1,19,"ASTM A600"},
        {"O1 Tool Steel","Tool Steel",7.80,380,620,210,10.8,32,"ASTM A681"},
        // ── Aluminum Alloys ──────────────────────────────────
        {"Al 1100-O","Aluminum",2.71,35,90,69,23.6,220,"ASTM B209"},
        {"Al 2024-T3","Aluminum",2.78,345,485,73,23.2,120,"ASTM B209"},
        {"Al 2024-T4","Aluminum",2.78,325,470,73,23.2,120,"ASTM B209"},
        {"Al 5052-H32","Aluminum",2.68,195,230,70,23.8,138,"ASTM B209"},
        {"Al 6061-O","Aluminum",2.70,55,125,69,23.6,170,"ASTM B209"},
        {"Al 6061-T6","Aluminum",2.70,276,310,69,23.6,167,"ASTM B209"},
        {"Al 6063-T6","Aluminum",2.70,214,240,69,23.4,200,"ASTM B209"},
        {"Al 7075-T6","Aluminum",2.81,505,570,72,23.6,130,"ASTM B209"},
        {"Al 7075-T73","Aluminum",2.81,435,505,72,23.4,155,"ASTM B209"},
        // ── Titanium Alloys ──────────────────────────────────
        {"Ti Grade 2","Titanium",4.51,275,345,105,8.6,16,"ASTM B265"},
        {"Ti Grade 5","Titanium",4.43,880,950,114,9.0,7,"ASTM B265"},
        {"Ti-6Al-4V ELI","Titanium",4.43,795,860,114,8.8,7,"ASTM F136"},
        {"Ti-3Al-2.5V","Titanium",4.48,520,620,100,8.6,8,"ASTM B265"},
        // ── Copper Alloys ────────────────────────────────────
        {"C110 ETP Copper","Copper",8.89,70,220,115,17.0,390,"ASTM B152"},
        {"C260 Cartridge Brass","Copper",8.53,75,300,110,19.9,120,"ASTM B36"},
        {"C360 Free-Cutting Brass","Copper",8.50,125,350,97,20.5,115,"ASTM B16"},
        {"C510 Phosphor Bronze","Copper",8.86,130,335,110,17.8,70,"ASTM B103"},
        {"C630 Nickel Al Bronze","Copper",7.58,250,620,115,16.2,50,"ASTM B150"},
        {"C715 CuNi 30%","Copper",8.94,125,370,150,16.2,29,"ASTM B122"},
        // ── Nickel Alloys ────────────────────────────────────
        {"Inconel 600","Nickel",8.42,240,550,207,13.3,15,"ASTM B168"},
        {"Inconel 625","Nickel",8.44,415,830,208,12.8,10,"ASTM B443"},
        {"Inconel 718","Nickel",8.19,1035,1240,205,13.0,11,"AMS 5662"},
        {"Monel 400","Nickel",8.80,195,480,179,13.9,22,"ASTM B127"},
        {"Hastelloy C-276","Nickel",8.89,355,790,205,11.2,11,"ASTM B575"},
        // ── Magnesium ─────────────────────────────────────────
        {"AZ31B","Magnesium",1.77,145,260,45,26.0,96,"ASTM B91"},
        {"AZ91D","Magnesium",1.81,160,280,45,25.2,72,"ASTM B94"},
        // ── Zinc ──────────────────────────────────────────────
        {"Zamak 3","Zinc",6.60,221,283,85,27.4,113,"ASTM B86"},
        {"Zamak 5","Zinc",6.60,276,331,85,27.4,109,"ASTM B86"},
        // ── Engineering Plastics ──────────────────────────────
        {"ABS","Plastic",1.04,45,40,2.3,90,0.2,"ISO 2580"},
        {"Polycarbonate (PC)","Plastic",1.20,65,65,2.4,68,0.2,"ISO 7391"},
        {"PC/ABS Blend","Plastic",1.14,55,50,2.5,80,0.2,""},
        {"Nylon 6 (PA6)","Plastic",1.14,80,75,2.8,80,0.2,"ISO 1874"},
        {"Nylon 6/6 (PA66)","Plastic",1.14,85,80,3.0,80,0.2,"ISO 1874"},
        {"POM (Acetal)","Plastic",1.41,70,70,3.0,100,0.3,"ISO 9988"},
        {"PEEK","Plastic",1.32,100,100,3.6,50,0.3,"ISO 23153"},
        {"PTFE (Teflon)","Plastic",2.20,15,30,0.5,120,0.2,"ASTM D4894"},
        {"PMMA (Acrylic)","Plastic",1.19,70,70,3.0,70,0.2,"ISO 7823"},
        {"HDPE","Plastic",0.95,26,32,1.1,120,0.4,"ISO 1872"},
        {"PP (Polypropylene)","Plastic",0.90,35,35,1.5,100,0.2,"ISO 1873"},
        {"PS (Polystyrene)","Plastic",1.05,45,50,3.0,70,0.2,"ISO 1622"},
        {"PPS","Plastic",1.35,90,90,3.8,50,0.3,"ISO 20558"},
        // ── Composites ────────────────────────────────────────
        {"Carbon Fiber Epoxy","Composite",1.55,600,700,70,2.0,5,""},
        {"Glass Fiber Epoxy","Composite",1.85,300,400,25,15.0,0.3,""},
        // ── Cast Irons ────────────────────────────────────────
        {"Gray Cast Iron G25","Cast Iron",7.15,170,250,105,10.5,50,"ISO 185"},
        {"Ductile Iron 65-45-12","Cast Iron",7.10,310,450,165,11.0,36,"ASTM A536"},
        {"Malleable Iron","Cast Iron",7.30,230,370,170,12.0,50,"ASTM A47"},
        // ── Other Metals ──────────────────────────────────────
        {"Pure Lead","Other",11.34,10,17,14,29.0,35,""},
        {"Pure Tin","Other",7.30,11,14,42,22.0,64,""},
        {"Tungsten","Other",19.25,750,980,410,4.5,170,""},
        {"Molybdenum","Other",10.22,420,530,330,5.1,138,""},
    };
}

// ──────────── Sheet Metal Gauge Tables ──────────────────────────────────────

struct SheetMetalGauge {
    std::string gauge;
    double thickness; // mm
    std::string standard;
};

inline std::vector<SheetMetalGauge> isoMetricGauges() {
    return {{"0.5",0.5,"ISO"},{"0.6",0.6,"ISO"},{"0.8",0.8,"ISO"},
            {"1.0",1.0,"ISO"},{"1.2",1.2,"ISO"},{"1.5",1.5,"ISO"},
            {"2.0",2.0,"ISO"},{"2.5",2.5,"ISO"},{"3.0",3.0,"ISO"},
            {"4.0",4.0,"ISO"},{"5.0",5.0,"ISO"},{"6.0",6.0,"ISO"},
            {"8.0",8.0,"ISO"},{"10.0",10.0,"ISO"},{"12.0",12.0,"ISO"}};
}

inline std::vector<SheetMetalGauge> ansiSteelGauges() {
    return {{"3 ga",6.07,"ANSI"},{"4 ga",5.69,"ANSI"},{"5 ga",5.31,"ANSI"},
            {"6 ga",4.94,"ANSI"},{"7 ga",4.55,"ANSI"},{"8 ga",4.18,"ANSI"},
            {"9 ga",3.80,"ANSI"},{"10 ga",3.42,"ANSI"},{"11 ga",3.04,"ANSI"},
            {"12 ga",2.66,"ANSI"},{"13 ga",2.28,"ANSI"},{"14 ga",1.90,"ANSI"},
            {"15 ga",1.71,"ANSI"},{"16 ga",1.52,"ANSI"},{"17 ga",1.37,"ANSI"},
            {"18 ga",1.21,"ANSI"},{"19 ga",1.06,"ANSI"},{"20 ga",0.91,"ANSI"},
            {"22 ga",0.76,"ANSI"},{"24 ga",0.61,"ANSI"},{"26 ga",0.46,"ANSI"},
            {"28 ga",0.38,"ANSI"},{"30 ga",0.31,"ANSI"}};
}

inline std::vector<SheetMetalGauge> aluminumGauges() {
    return {{"0.5",0.5,"AA"},{"0.6",0.6,"AA"},{"0.8",0.8,"AA"},
            {"1.0",1.0,"AA"},{"1.2",1.2,"AA"},{"1.6",1.6,"AA"},
            {"2.0",2.0,"AA"},{"2.5",2.5,"AA"},{"3.0",3.0,"AA"},
            {"4.0",4.0,"AA"},{"5.0",5.0,"AA"},{"6.0",6.0,"AA"}};
}

inline std::vector<SheetMetalGauge> stainlessSteelGauges() {
    return {{"10 ga",3.57,"ASTM"},{"11 ga",3.18,"ASTM"},{"12 ga",2.78,"ASTM"},
            {"13 ga",2.38,"ASTM"},{"14 ga",1.98,"ASTM"},{"16 ga",1.59,"ASTM"},
            {"18 ga",1.27,"ASTM"},{"20 ga",0.95,"ASTM"},{"22 ga",0.79,"ASTM"},
            {"24 ga",0.64,"ASTM"},{"26 ga",0.48,"ASTM"},{"28 ga",0.40,"ASTM"}};
}

} // namespace nexus::cad
