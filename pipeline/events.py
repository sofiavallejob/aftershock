"""Curated event list for the Aftershock pack.

Each entry gives an approximate origin time; the exact origin (time, location,
depth, magnitude) is taken from the USGS ComCat catalogue at build time.
The tectonic setting label is the reason the event is in the pack: the pack is
meant to cover the settings that produce audibly different codas.
"""

SETTINGS = {
    "interface": "Subduction interface",
    "intraslab": "Intermediate-depth slab",
    "deep": "Deep slab (> 300 km)",
    "collision": "Continental collision / thrust",
    "strikeslip": "Continental strike-slip",
    "europe": "Mediterranean crustal",
    "oceanic": "Oceanic intraplate / transform",
    "craton": "Stable continental interior",
    "volcanic": "Volcanic",
}

# (id, name, approx UTC origin, setting)
EVENTS = [
    ("tohoku2011",   "Tohoku, Japan",            "2011-03-11T05:46:24", "interface"),
    ("maule2010",    "Maule, Chile",             "2010-02-27T06:34:11", "interface"),
    ("illapel2015",  "Illapel, Chile",           "2015-09-16T22:54:32", "interface"),
    ("iquique2014",  "Iquique, Chile",           "2014-04-01T23:46:47", "interface"),
    ("chignik2021",  "Chignik, Alaska",          "2021-07-29T06:15:49", "interface"),
    ("kamchatka2025","Kamchatka, Russia",        "2025-07-29T23:24:50", "interface"),
    ("chiapas2017",  "Chiapas, Mexico",          "2017-09-08T04:49:19", "intraslab"),
    ("puebla2017",   "Puebla, Mexico",           "2017-09-19T18:14:38", "intraslab"),
    ("hindukush2015","Hindu Kush, Afghanistan",  "2015-10-26T09:09:42", "intraslab"),
    ("peru2019",     "Northern Peru",            "2019-05-26T07:41:15", "intraslab"),
    ("fiji2018",     "Fiji deep",                "2018-08-19T00:19:40", "deep"),
    ("fiji2018b",    "Fiji deep II",             "2018-09-06T15:49:18", "deep"),
    ("okhotsk2013",  "Sea of Okhotsk",           "2013-05-24T05:44:48", "deep"),
    ("bonin2015",    "Bonin Islands",            "2015-05-30T11:23:02", "deep"),
    ("gorkha2015",   "Gorkha, Nepal",            "2015-04-25T06:11:25", "collision"),
    ("wenchuan2008", "Wenchuan, China",          "2008-05-12T06:28:01", "collision"),
    ("kashmir2005",  "Kashmir, Pakistan",        "2005-10-08T03:50:40", "collision"),
    ("noto2024",     "Noto, Japan",              "2024-01-01T07:10:09", "collision"),
    ("hualien2024",  "Hualien, Taiwan",          "2024-04-02T23:58:11", "collision"),
    ("turkiye2023",  "Kahramanmaras, Turkiye",   "2023-02-06T01:17:34", "strikeslip"),
    ("ridgecrest2019","Ridgecrest, California",  "2019-07-06T03:19:53", "strikeslip"),
    ("kaikoura2016", "Kaikoura, New Zealand",    "2016-11-13T11:02:56", "strikeslip"),
    ("haiti2010",    "Haiti",                    "2010-01-12T21:53:10", "strikeslip"),
    ("myanmar2025",  "Mandalay, Myanmar",        "2025-03-28T06:20:52", "strikeslip"),
    ("petrinja2020", "Petrinja, Croatia",        "2020-12-29T11:19:54", "europe"),
    ("norcia2016",   "Norcia, Italy",            "2016-10-30T06:40:18", "europe"),
    ("durres2019",   "Durres, Albania",          "2019-11-26T02:54:12", "europe"),
    ("samos2020",    "Samos, Aegean Sea",        "2020-10-30T11:51:27", "europe"),
    ("sumatra2012",  "Wharton Basin, Indian Ocean", "2012-04-11T08:38:36", "oceanic"),
    ("ascension2016","North of Ascension Island","2016-08-29T04:29:57", "oceanic"),
    ("petermann2016","Petermann Ranges, Australia","2016-05-20T18:14:04", "craton"),
    ("botswana2017", "Moiyabana, Botswana",      "2017-04-03T17:40:18", "craton"),
    ("mineral2011",  "Mineral, Virginia",        "2011-08-23T17:51:04", "craton"),
    ("kilauea2018",  "Kilauea, Hawaii",          "2018-05-04T22:32:54", "volcanic"),
    ("etna2018",     "Mount Etna, Italy",        "2018-12-26T02:19:14", "volcanic"),
]

# distance bands (degrees): near, regional, teleseismic
BANDS = [("near", 0.0, 3.0), ("regional", 3.0, 15.0), ("tele", 20.0, 60.0)]
