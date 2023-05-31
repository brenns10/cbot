Search.setIndex({"docnames": ["Plugins", "Plugins-2", "Tooling", "User", "admin", "dev", "index"], "filenames": ["Plugins.md", "Plugins-2.md", "Tooling.md", "User.md", "admin.rst", "dev.rst", "index.rst"], "titles": ["How to write a CBot Plugin", "Plugins - Advanced Topics", "Dev Tooling", "Users", "Administering CBot", "Development", "Welcome to CBot\u2019s Documentation!"], "terms": {"ha": [0, 1, 3, 4], "quit": 0, "pleasant": 0, "system": [0, 1, 4], "If": [0, 1, 2, 3, 4], "you": [0, 1, 2, 3, 4], "re": [0, 2, 3], "familiar": 0, "c": [0, 1, 4, 6], "abl": [0, 1, 3], "extend": 0, "bot": [0, 1, 3, 4], "bit": [0, 1], "follow": [0, 1, 4], "thi": [0, 1, 2, 3, 4, 6], "guid": 0, "learn": [0, 5], "basic": [0, 1, 3], "develop": [0, 6], "To": [0, 4, 5], "drive": 0, "tutori": 0, "we": [0, 1, 4], "try": 0, "build": [0, 2], "hello": [0, 1, 3], "world": 0, "simpli": [0, 4], "repli": [0, 1], "ani": [0, 1, 4], "messag": [0, 3], "which": [0, 1, 3, 4], "sai": 0, "respond": [0, 3], "while": 0, "itself": [0, 1], "i": [0, 1, 3, 4, 6], "pretti": [0, 1], "silli": 0, "onc": [0, 4], "can": [0, 1, 3, 4], "do": [0, 1, 3], "ll": [0, 1, 4], "start": [0, 3], "make": [0, 1, 2, 3], "much": 0, "more": [0, 1, 3], "interest": [0, 3, 4], "stuff": 0, "As": 0, "prerequisit": 0, "assum": 0, "ar": [0, 1, 3, 4], "us": [0, 1, 2, 4, 6], "The": [0, 1, 4], "best": [0, 3], "wai": 0, "tree": 0, "within": 0, "repositori": 0, "just": [0, 1, 3], "creat": [0, 1, 4], "file": [0, 1, 2, 3, 4], "asid": 0, "whatev": [0, 1], "name": [0, 1, 3, 4, 6], "choos": [0, 3], "your": [0, 1, 2, 3, 4], "includ": [0, 1, 3, 4], "extens": [0, 6], "should": [0, 1, 2, 4], "valid": 0, "identifi": 0, "less": 0, "allow": [0, 1, 3, 4], "configur": [0, 3, 6], "here": [0, 3, 4], "complet": [0, 2], "blank": 0, "minim": 0, "doe": 0, "noth": 0, "h": [0, 1], "static": 0, "int": 0, "cbot_plugin": 0, "config_setting_t": 0, "conf": 0, "return": [0, 1], "0": [0, 3, 4], "cbot_plugin_op": 0, "save": 0, "add": [0, 2, 3, 4], "list": [0, 1, 3], "meson": [0, 2, 4], "root": [0, 2], "when": [0, 3], "recompil": 0, "ninja": [0, 4], "detect": 0, "chang": [0, 1, 3, 4], "reconfigur": 0, "Then": [0, 1], "everyth": [0, 4], "along": [0, 1], "new": [0, 1, 2], "find": [0, 4], "so": [0, 1, 3], "abov": [0, 3], "contain": [0, 1, 4], "an": [0, 1, 2, 3, 4, 6], "empti": [0, 4], "declar": 0, "These": 0, "item": [0, 1], "search": [0, 6], "symbol": 0, "expect": 0, "type": 0, "see": [0, 1, 3, 4], "definit": [0, 2], "inc": [0, 1], "docstr": 0, "remov": [0, 3], "char": 0, "descript": 0, "config": [0, 4], "void": 0, "unload": 0, "help": [0, 1, 4], "sc_charbuf": 0, "cb": 0, "all": [0, 1, 3, 4], "field": [0, 1], "option": [0, 4], "except": 0, "one": [0, 1, 3], "focu": 0, "right": 0, "now": [0, 1], "initi": 0, "task": 0, "place": 0, "where": 0, "tell": [0, 3], "would": [0, 1], "like": [0, 1, 2, 3, 4], "certain": 0, "event": [0, 1], "don": [0, 1, 3], "t": [0, 1, 3, 4], "never": 0, "get": [0, 1, 3, 4], "call": [0, 1], "again": 0, "our": 0, "": [0, 1, 3, 4], "perfectli": 0, "There": [0, 1, 3], "two": [0, 3], "import": [0, 4], "argument": [0, 1], "pointer": 0, "look": [0, 1, 3], "data": 0, "instanc": [0, 3, 4], "alloc": 0, "each": [0, 1, 4], "point": [0, 1], "variabl": 0, "defin": 0, "bottom": 0, "null": 0, "store": 0, "later": 0, "final": 0, "main": [0, 1], "most": [0, 3], "action": [0, 1, 3], "send": [0, 3], "mai": [0, 1], "modifi": 0, "want": [0, 1, 4], "current": [0, 3], "dynam": [0, 5], "behavior": 0, "ok": [0, 3], "reli": 0, "leav": 0, "alon": 0, "overwrit": 0, "api": [0, 4, 5], "becom": [0, 3], "difficult": [0, 3], "second": [0, 3], "element": 0, "from": [0, 1, 3, 6], "libconfig": [0, 4], "won": [0, 1], "cover": 0, "mention": 0, "specif": [0, 3], "access": 0, "know": [0, 1], "what": [0, 1, 3], "let": 0, "u": 0, "handl": [0, 1], "few": [0, 3], "differ": [0, 1], "enum": 0, "cbot_event_typ": 0, "cbot_messag": 0, "cbot_address": 0, "trigger": [0, 3], "everi": [0, 3], "singl": 0, "direct": [0, 3], "channel": [0, 3], "etc": [0, 1, 2], "special": 0, "onli": [0, 1, 3], "subset": 0, "those": 0, "address": 0, "some": [0, 3, 4], "exampl": [0, 1, 4], "three": 0, "gener": [0, 1, 2, 3], "same": [0, 1, 4], "content": [0, 6], "trim": 0, "author": [0, 4], "without": [0, 1], "about": [0, 1, 3], "usernam": [0, 3], "have": [0, 1, 3, 4], "filter": 0, "out": [0, 1], "irrelev": 0, "In": [0, 1, 4], "case": 0, "For": [0, 4], "specifi": [0, 4], "regular": [0, 1], "express": [0, 1], "further": 0, "full": 0, "match": [0, 1, 3], "regex": [0, 1], "nice": 0, "easi": [0, 1], "A": [0, 1, 4], "take": [0, 1, 3], "cbot_ev": 0, "purpos": 0, "version": [0, 1, 2, 4], "cast": 0, "cbot_message_ev": 0, "string": [0, 1, 4], "other": [0, 1, 3, 4], "discuss": 0, "user": [0, 6], "given": [0, 3], "It": [0, 1, 2, 3, 4, 6], "With": 0, "inform": 0, "registr": [0, 5], "clear": 0, "cbot_handl": 0, "cbot_regist": 0, "cbot_handler_t": 0, "first": [0, 4], "put": 0, "togeth": 0, "hypothet": 0, "say_hello": 0, "cbot_send": 0, "note": [0, 1], "becaus": [0, 1, 3], "downsid": 0, "had": 0, "wa": 0, "remaind": 0, "seem": 0, "obviou": [0, 1], "origin": 0, "came": 0, "simpl": 0, "ad": 0, "line": 0, "group": [0, 3], "notic": 0, "insid": 0, "go": [0, 1], "directli": 0, "neat": 0, "anywai": 0, "yet": 0, "good": [0, 1, 3], "baselin": 0, "test": [0, 4], "outsid": 0, "irc": [0, 1, 3, 4, 6], "cli": [0, 4], "cfg": [0, 4], "too": 0, "desir": 0, "stdin": 0, "backend": 0, "plugin_dir": 0, "db": [0, 1], "sqlite3": [0, 4], "after": 0, "chat": [0, 3], "error": 0, "been": [0, 3], "attempt": 0, "respons": [0, 3], "And": [0, 1], "ve": [0, 3], "written": 0, "next": [0, 3, 4], "up": [0, 3, 4], "brows": [0, 1], "through": [0, 6], "md": 0, "advanc": [0, 5, 6], "topic": [0, 5, 6], "great": [0, 1, 4], "document": [1, 3, 4], "particularli": 1, "detail": [1, 3], "intend": [1, 3], "give": [1, 3], "idea": 1, "tool": [1, 5, 6], "cbot": [1, 5], "write": [1, 5, 6], "rather": 1, "than": [1, 3], "code": [1, 2, 3], "read": 1, "check": 1, "librari": [1, 4, 5], "sourc": [1, 6], "doc": 1, "commonli": 1, "sc": [1, 2, 4], "collect": 1, "heavili": 1, "link": 1, "charact": 1, "buffer": 1, "built": [1, 4], "against": 1, "lwt": 1, "solut": 1, "lot": [1, 3], "o": [1, 4], "heavi": 1, "maintain": [1, 4], "welcom": 1, "provid": [1, 4], "coupl": 1, "conveni": 1, "authorit": 1, "refer": 1, "below": [1, 6], "sqlkarma": 1, "sqlknow": 1, "regist": [1, 5], "struct": [1, 5], "cbot_db_tabl": 1, "why": 1, "schema": 1, "decid": 1, "increment": 1, "its": [1, 3], "date": 1, "arrai": 1, "alter": 1, "statement": 1, "old": 1, "run": [1, 2, 3, 5, 6], "tediou": 1, "macro": 1, "execut": 1, "result": 1, "bother": 1, "mani": 1, "actual": [1, 3, 4], "async": 1, "program": [1, 2, 4], "awesom": [1, 3], "model": 1, "oper": 1, "switch": 1, "between": [1, 4], "sever": [1, 4], "whenev": 1, "thei": [1, 3, 4], "work": [1, 3, 5], "done": 1, "cooper": 1, "multitask": 1, "typic": 1, "block": 1, "wait": 1, "plenti": 1, "opportun": 1, "cbot_get_lwt_ctx": 1, "retriev": 1, "context": 1, "object": [1, 4], "ahead": 1, "launch": 1, "behav": 1, "well": [1, 3, 4, 5], "hog": 1, "cpu": 1, "mainli": 1, "meant": 1, "sure": [1, 2, 3], "safe": 1, "integr": 1, "need": [1, 2, 3, 4], "off": [1, 3], "annoi": [1, 3], "excel": 1, "connect": [1, 4], "mode": 1, "multi": 1, "parallel": 1, "ton": 1, "callback": 1, "order": 1, "must": [1, 4], "separ": 1, "curl": [1, 4], "overview": 1, "curl_easy_perform": 1, "cbot_curl_perform": 1, "weather": 1, "sometim": 1, "command": [1, 3, 4], "drag": 1, "were": 1, "delimit": 1, "whitespac": 1, "support": [1, 2, 4], "quot": 1, "tok": 1, "implement": [1, 3, 6], "src": 1, "worth": 1, "love": 1, "python": 1, "f": [1, 3], "anyth": 1, "nearli": 1, "70": 1, "base": 1, "charbuf": 1, "target": 1, "my": 1, "mynam": 1, "url": 1, "fill": 1, "curli": 1, "brace": 1, "correct": 1, "valu": 1, "isn": 1, "usual": [1, 3, 4], "stick": 1, "printf": 1, "famili": 1, "But": [1, 3], "even": [1, 3], "output": 1, "cbot_format": 1, "formatt": [1, 2], "expans": 1, "append": 1, "builder": 1, "editor": 2, "e": [2, 3], "g": [2, 3], "vim": 2, "vscode": 2, "compile_command": 2, "json": 2, "symlink": 2, "veri": [2, 4], "enabl": 2, "jump": 2, "realli": [2, 3], "feel": 2, "2020": 2, "repo": 2, "pre": 2, "automat": 2, "Be": 2, "instal": [2, 6], "git": [2, 4], "checkout": [2, 4], "subproject": 2, "depend": [2, 3], "lib": [2, 4], "featur": [2, 3], "pin": 2, "minimum": 2, "mayb": 3, "brought": 3, "explain": 3, "might": 3, "guidanc": 3, "listen": 3, "sent": 3, "howev": [3, 4], "requir": [3, 4], "either": 3, "tag": 3, "platform": 3, "prefix": [3, 4], "cbut": 3, "exact": 3, "ask": 3, "person": 3, "who": 3, "curiou": 3, "section": 3, "divid": 3, "disabl": 3, "me": 3, "30": 3, "san": 3, "francisco": 3, "7": 3, "4": [3, 5], "america": 3, "wish": 3, "happi": 3, "3": [3, 5], "14": 3, "pi": 3, "17": 3, "saint": 3, "patrick": 3, "12": 3, "25": 3, "jesu": 3, "delet": 3, "1": [3, 5], "record": 3, "At": 3, "time": 3, "9am": 3, "pacif": 3, "On": 3, "last": 3, "dai": 3, "month": 3, "also": [3, 4], "pleas": [3, 4, 6], "http": [3, 4, 5], "brenns10": 3, "github": [3, 4], "io": 3, "html": 3, "stephen": 3, "word": 3, "subtract": 3, "queri": [3, 4, 5], "particular": 3, "highest": 3, "random": 3, "pattern": 3, "slackbot": 3, "befor": 3, "unfortun": 3, "mean": 3, "standard": 3, "set": [3, 4], "That": 3, "said": 3, "common": 3, "ones": 3, "morn": 3, "afternoon": 3, "night": 3, "x": 3, "lod": 3, "\u0ca0_\u0ca0": 3, "ping": 3, "pong": 3, "magic8": 3, "magic": 3, "8": [3, 4], "ball": 3, "altern": 3, "syntax": 3, "8ball": 3, "suck": 3, "variant": 3, "stupid": 3, "taylor": 3, "swift": 3, "94105": 3, "48": 3, "19mph": 3, "zip": 3, "locat": 3, "emoji": 3, "rich": 3, "report": 3, "condit": 3, "straight": 3, "wttr": 3, "mostli": 3, "cool": 3, "demonstr": 3, "how": [3, 5, 6], "technic": 3, "though": 3, "power": 3, "sit": 3, "around": 3, "keep": 3, "emot": 3, "uninterest": 3, "repeat": 3, "ircctl": 3, "instruct": 3, "perform": 3, "higher": 3, "privileg": [3, 4], "behalf": 3, "log": 3, "supposedli": 3, "haven": 3, "year": 3, "promis": 3, "whether": 3, "notif": 3, "everyon": 3, "abil": 3, "mundan": 3, "releas": 4, "binari": 4, "alpin": 4, "linux": 4, "apk": 4, "untrust": 4, "imag": 4, "publish": 4, "hub": 4, "sinc": 4, "v0": 4, "longer": 4, "signald": 4, "bundl": 4, "signal": [4, 6], "setup": 4, "yourself": 4, "via": 4, "compos": 4, "unpack": 4, "revis": 4, "compil": [4, 5], "gcc": 4, "clang": 4, "pars": 4, "libcurl": [4, 5], "libmicrohttpd": 4, "server": [4, 5], "libirccli": 4, "vendor": 4, "doesn": 4, "state": 4, "necessari": 4, "variou": 4, "ubuntu": 4, "sudo": 4, "apt": 4, "dev": [4, 5, 6], "libcurl4": 4, "openssl": 4, "libsqlite3": 4, "arch": 4, "pacman": 4, "sy": 4, "sqlite": [4, 5], "project": 4, "directori": 4, "dure": 4, "step": [4, 5], "download": 4, "cannot": 4, "happen": 4, "snappi": 4, "sampl": 4, "plugin": [4, 5, 6], "basi": 4, "thing": 4, "ought": 4, "whichev": 4, "correspond": 4, "top": 4, "level": 4, "host": 4, "hostnam": 4, "ssl": 4, "port": 4, "integ": 4, "password": 4, "avaial": 4, "phone": 4, "number": 4, "account": 4, "signald_socket": 4, "filenam": 4, "auth": 4, "ignore_dm": 4, "ignor": 4, "dm": 4, "beyond": 4, "load": [4, 5], "map": 4, "dict": 4, "languag": 5, "ccl": 5, "clangd": 5, "commit": 5, "hook": 5, "updat": 5, "structur": 5, "2": 5, "op": 5, "handler": 5, "function": 5, "5": 5, "persist": 5, "storag": 5, "databas": 5, "tabl": 5, "migrat": 5, "lightweight": 5, "thread": 5, "request": 5, "token": 5, "format": 5, "flexibl": 6, "chatbot": 6, "thu": 6, "navig": 6, "talk": 6, "administ": 6, "distribut": 6, "packag": 6, "docker": 6, "index": 6, "page": 6}, "objects": {}, "objtypes": {}, "objnames": {}, "titleterms": {"how": 0, "write": 0, "cbot": [0, 3, 4, 6], "plugin": [0, 1, 3], "step": 0, "1": 0, "structur": 0, "2": 0, "op": 0, "struct": 0, "load": 0, "3": 0, "regist": 0, "handler": 0, "4": 0, "function": [0, 1], "5": 0, "compil": 0, "run": [0, 4], "advanc": 1, "topic": 1, "api": 1, "To": 1, "learn": 1, "persist": 1, "storag": 1, "sqlite": 1, "databas": 1, "tabl": [1, 6], "registr": 1, "migrat": 1, "queri": 1, "lightweight": 1, "thread": 1, "http": 1, "request": 1, "libcurl": 1, "token": 1, "dynam": 1, "format": 1, "dev": 2, "tool": 2, "languag": 2, "server": 2, "ccl": 2, "clangd": 2, "work": 2, "well": 2, "commit": 2, "hook": 2, "updat": 2, "librari": 2, "user": 3, "talk": 3, "commonli": 3, "enabl": 3, "aqi": 3, "birthdai": 3, "greet": 3, "help": 3, "karma": 3, "repli": 3, "know": 3, "weather": 3, "Not": 3, "frequent": 3, "us": 3, "administ": 4, "instal": 4, "distribut": 4, "packag": 4, "docker": 4, "from": 4, "sourc": 4, "depend": 4, "build": 4, "initi": 4, "configur": 4, "choos": 4, "backend": 4, "develop": 5, "welcom": 6, "": 6, "document": 6, "indic": 6}, "envversion": {"sphinx.domains.c": 2, "sphinx.domains.changeset": 1, "sphinx.domains.citation": 1, "sphinx.domains.cpp": 8, "sphinx.domains.index": 1, "sphinx.domains.javascript": 2, "sphinx.domains.math": 2, "sphinx.domains.python": 3, "sphinx.domains.rst": 2, "sphinx.domains.std": 2, "sphinx.ext.todo": 2, "sphinx.ext.viewcode": 1, "sphinx": 57}, "alltitles": {"Plugins - Advanced Topics": [[1, "plugins-advanced-topics"]], "APIs To Learn": [[1, "apis-to-learn"]], "Persistent Storage: Sqlite Database": [[1, "persistent-storage-sqlite-database"]], "Table registration and migration": [[1, "table-registration-and-migration"]], "Query functions": [[1, "query-functions"]], "Lightweight Threads": [[1, "lightweight-threads"]], "HTTP Requests: libcurl": [[1, "http-requests-libcurl"]], "Tokenizing": [[1, "tokenizing"]], "Dynamic Formatting": [[1, "dynamic-formatting"]], "Dev Tooling": [[2, "dev-tooling"]], "Language Server (ccls or clangd work well)": [[2, "language-server-ccls-or-clangd-work-well"]], "Commit Hooks": [[2, "commit-hooks"]], "Updating Libraries": [[2, "updating-libraries"]], "How to write a CBot Plugin": [[0, "how-to-write-a-cbot-plugin"]], "Step 1: Plugin structure": [[0, "step-1-plugin-structure"]], "Step 2: ops Struct and load()": [[0, "step-2-ops-struct-and-load"]], "Step 3: Registering handlers": [[0, "step-3-registering-handlers"]], "Step 4: Handler function": [[0, "step-4-handler-function"]], "Step 5: Compile and run": [[0, "step-5-compile-and-run"]], "Development": [[5, "development"]], "Administering CBot": [[4, "administering-cbot"]], "Installing (Distribution Packages)": [[4, "installing-distribution-packages"]], "Installing (Docker)": [[4, "installing-docker"]], "Installing (From Source)": [[4, "installing-from-source"]], "Dependencies": [[4, "dependencies"]], "Building": [[4, "building"]], "Initial Run": [[4, "initial-run"]], "Configuring": [[4, "configuring"]], "Choosing & Configuring Backend": [[4, "choosing-configuring-backend"]], "Configuring CBot": [[4, "configuring-cbot"]], "Users": [[3, "users"]], "Talking to CBot": [[3, "talking-to-cbot"]], "Plugins": [[3, "plugins"]], "Commonly Enabled Plugins": [[3, "commonly-enabled-plugins"]], "aqi": [[3, "aqi"]], "birthday": [[3, "birthday"]], "greet": [[3, "greet"]], "help": [[3, "help"]], "karma": [[3, "karma"]], "reply": [[3, "reply"]], "know": [[3, "know"]], "weather": [[3, "weather"]], "Plugins Not Frequently Used": [[3, "plugins-not-frequently-used"]], "Welcome to CBot\u2019s Documentation!": [[6, "welcome-to-cbot-s-documentation"]], "CBot": [[6, "cbot"]], "Indices and tables": [[6, "indices-and-tables"]]}, "indexentries": {}})