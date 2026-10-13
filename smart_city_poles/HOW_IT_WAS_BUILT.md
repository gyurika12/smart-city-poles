# Πώς φτιάχτηκε το add-on «Smart City Poles»

Αυτό το κείμενο εξηγεί βήμα-βήμα πώς σχεδιάστηκε και υλοποιήθηκε το
Home Assistant add-on του project — τι κάνει κάθε αρχείο, γιατί διαλέχτηκε
κάθε λύση και πώς μπορείς να το επεκτείνεις.

---

## 1. Τι είναι ένα Home Assistant add-on

Ένα add-on είναι ένα **Docker container** που ο Supervisor του Home Assistant
χτίζει και τρέχει στο ίδιο μηχάνημα. Κάθε add-on είναι ένας φάκελος με:

| Αρχείο | Ρόλος |
| --- | --- |
| `config.yaml` | Το «διαβατήριο»: όνομα, έκδοση, αρχιτεκτονικές, δικαιώματα, options |
| `build.yaml` | Ποιο base image ανά αρχιτεκτονική CPU |
| `Dockerfile` | Πώς χτίζεται το image |
| `rootfs/` | Αρχεία που αντιγράφονται αυτούσια μέσα στο container |
| `translations/` | Περιγραφές των options στο UI (EN/EL) |
| `README.md` / `DOCS.md` / `CHANGELOG.md` | Σελίδα του store, τεκμηρίωση, ιστορικό |

Για να μπορεί κάποιος να το εγκαταστήσει από τον Add-on Store, το GitHub repo
χρειάζεται στη ρίζα του ένα **`repository.yaml`** — αυτό το κάνει
«add-on repository».

---

## 2. Οι δύο στόχοι και πώς καλύφθηκαν

### Στόχος Α — Dashboard στο sidebar (Ingress)

Το υπάρχον `home_assistant/www/dashboard_kolones.html` είναι ένα **αυτόνομο
single-file dashboard** (Leaflet + Chart.js) που μιλάει στο Home Assistant API
με `fetch`/`WebSocket` στο **ίδιο origin**, παίρνοντας το token από τη
τρέχουσα σύνδεση του χρήστη.

Αυτό ήταν ιδανικό για το add-on, γιατί το **ingress** σερβίρει τη σελίδα μέσα
από το ίδιο origin του Home Assistant (`/api/hassio_ingress/...`). Άρα ο
κώδικας του dashboard **δεν χρειάστηκε καμία αλλαγή** — απλώς σερβίρεται
στατικά από έναν **nginx** μέσα στο container:

```
rootfs/
├── www/index.html                      ← το dashboard (αυτούσιο αντίγραφο)
└── etc/nginx/http.d/ingress.conf       ← server στη θύρα 8099
```

Το `try_files $uri $uri/ /index.html;` εξασφαλίζει ότι όποιο path κι αν ζητηθεί
(το ingress προσθέτει δικό του prefix), σερβίρεται πάντα η ίδια σελίδα.

Στο `config.yaml` το ενεργοποιούν:

```yaml
ingress: true
ingress_port: 8099
panel_icon: mdi:lightbulb-group
panel_title: Smart Poles
```

### Στόχος Β — Αυτόματη εγκατάσταση automations / energy package

Τα YAML του project (`kolones_automation.yaml`, `kolones_energy_test.yaml`)
έπρεπε να μπαίνουν **χωρίς copy-paste**. Η λύση: τα **packages** του
Home Assistant — αρχεία στο `/config/packages/` που περιέχουν πολλαπλά
domains μαζί (helpers, utility_meter, sensors, automation).

Το add-on δηλώνει:

```yaml
map:
  - config:rw          # βλέπει το /config του Home Assistant
homeassistant_api: true
role: homeassistant    # πλήρης πρόσβαση στο HA API (για check/reload/notify)
```

και ένα init script κάνει τα εξής σε κάθε εκκίνηση:

1. **Render templates → packages.** Τα `rootfs/usr/src/*.yaml.tpl` είναι τα
   YAML του project με **placeholders** (`__LIGHT1__`, `__SHELLY_ENERGY__`,
   `__LUX_SENSOR__`, `__START_DATE__`, ...). Το script τα αντικαθιστά με `sed`
   από τις τιμές των options και γράφει:
   - `/config/packages/smart_city_poles_energy.yaml`
     (helpers, timers, utility_meter, history_stats, template sensors)
   - `/config/packages/smart_city_poles_automation.yaml`
     (ο αυτοματισμός «Κολόνες - Πλήρης έλεγχος»)
2. **configuration.yaml.** Αν δεν φορτώνει τον φάκελο packages, προσθέτει
   `packages: !include_dir_named packages` κάτω από το `homeassistant:` —
   **αφού πρώτα πάρει backup** (`configuration.yaml.smart-poles-backup-…`).
   Η εισαγωγή γίνεται με `awk` (portable, κρατάει το indent σωστά).
3. **Check + reload.** Καλεί `POST /api/config/core/check_config` μέσω του
   supervisor proxy (`http://supervisor/core/api`, token στο
   `SUPERVISOR_TOKEN`). Αν είναι valid: `homeassistant.reload_all` — ή, αν
   πειράχτηκε το `configuration.yaml`, **persistent notification** ότι
   χρειάζεται πλήρες restart.
4. **Καθαρότητα.** Αν ο χρήστης κλείσει κάποιο install option, το
   αντίστοιχο generated αρχείο **διαγράφεται** (με ειδοποίηση για restart).

#### Γιατί packages και όχι API;

Οι αυτοματισμοί θα μπορούσαν να δημιουργηθούν και μέσω REST/WebSocket API,
αλλά τα **helpers, utility_meter, history_stats** γράφονται πολύ πιο
καθαρά και ελεγχόμενα ως YAML. Με τα packages όλα μαζί είναι ένα αρχείο,
versioned μέσα στο repo, και ο χρήστης μπορεί να τα δει στον file editor.

---

## 3. Αναλυτικά τα αρχεία

### `repository.yaml` (ρίζα του repo)

```yaml
name: Smart City Poles add-ons
url: https://github.com/gyurika12/smart-city-poles
maintainer: Gyurika (@gyurika12)
```

Αυτό και μόνο αρκεί ώστε το repo να προστεθεί στο Store ως repository.

### `smart_city_poles/config.yaml`

- `slug: smart_city_poles` — μοναδικό id (ίδιο με το όνομα του φακέλου).
- `arch:` — για ποιες CPU θα χτιστεί.
- `startup: application` — πότε το ξεκινά ο Supervisor.
- `options:` / `schema:` — οι ρυθμίσεις που βλέπει ο χρήστης στο Configuration
  tab. **Κάθε key του options υπάρχει και στο schema** με τύπο
  (`bool`, `str`, `match(...)` για την ημερομηνία).
- `init: false` — αφήνουμε το δικό μας s6-overlay να διαχειριστεί τις υπηρεσίες.

### `smart_city_poles/build.yaml` + `Dockerfile`

```yaml
build_from:
  amd64: ghcr.io/home-assistant/amd64-base:3.22
  aarch64: ghcr.io/home-assistant/aarch64-base:3.22
  ...
```

Τα επίσημα base images του Home Assistant έχουν έτοιμα: **s6-overlay v3**
(process supervisor), **bashio** (bash βιβλιοθήκη που διαβάζει τα options),
`curl`, `jq`. Το Dockerfile προσθέτει μόνο:

```dockerfile
RUN apk add --no-cache nginx && rm -f /etc/nginx/http.d/default.conf
COPY rootfs /
```

### s6-overlay services

```
rootfs/etc/s6-overlay/s6-rc.d/
├── init-smart-poles/        # oneshot: τρέχει μία φορά στην εκκίνηση
│   ├── type                 # "oneshot"
│   ├── up                   # δείχνει στο run script
│   └── run                  # ο εγκαταστάτης (βλ. §2Β)
├── nginx/                   # longrun: ο web server
│   ├── type                 # "longrun"
│   ├── run                  # exec nginx -g 'daemon off;'
│   ├── finish               # αν πέσει ο nginx, κατέβασε το container
│   └── dependencies         # init-smart-poles (τρέχει ΠΡΙΝ τον nginx)
└── user/contents.d/         # ποια services ενεργοποιούνται
    ├── init-smart-poles
    └── nginx
```

Τα scripts ξεκινούν με `#!/usr/bin/with-contenv bashio`, ώστε να έχουν
διαθέσιμα τα `bashio::config 'option_name'`, `bashio::log.info` κ.λπ.

### `translations/en.yaml` & `el.yaml`

Οι περιγραφές κάθε option στο UI του add-on — το Home Assistant διαλέγει
αυτόματα γλώσσα ανάλογα με το προφίλ του χρήστη.

---

## 4. Πώς έγινε η μετατροπή των YAML σε templates

1. **Energy package:** χειροκίνητη συγχώνευση των helpers από το
   `kolones_energy_test.yaml`, **συν** τα `timer.kolona1-3` που ο αυτοματισμός
   καλούσε αλλά δεν υπήρχαν πουθενά, με placeholders στα entity IDs.
2. **Automation:** μηχανική μετατροπή του `kolones_automation.yaml`:

   ```bash
   awk '/^alias:/{f=1} f' kolones_automation.yaml \  # από το alias: και κάτω
     | sed -e 's|light.yeelink_..._344_...|__LIGHT1__|g' ... \
     | awk 'NR==1{print "  - " $0; next} {print "    " $0}'  # indent μέσα σε automation:
   ```

3. **Έλεγχος:** τα rendered αρχεία περνούν από YAML parser και
   επιβεβαιώθηκε ότι ο αυτοματισμός βγαίνει ακέραιος
   (9 triggers, 4 choose branches) και δεν μένουν placeholders.

---

## 5. Δοκιμές που έγιναν

| Έλεγχος | Εργαλείο |
| --- | --- |
| Render templates + YAML validity | `python3` + PyYAML |
| Ακεραιότητα αυτοματισμού | μέτρηση triggers/branches μετά το render |
| Bash syntax του init script | `bash -n` |
| Λογική `configuration.yaml` (2 σενάρια) | awk/printf σε δοκιμαστικά αρχεία |
| options ↔ schema αντιστοίχιση | script ελέγχου |

Το Docker build γίνεται από τον Supervisor κατά την εγκατάσταση — δεν
χρειάζεται τοπικά.

---

## 6. Πώς να το επεκτείνεις

- **Νέο option:** πρόσθεσέ το σε `options:` + `schema:` στο `config.yaml`,
  περιγραφή στα `translations/*.yaml`, και διάβασέ το στο init script με
  `bashio::config 'το_option'`.
- **Αλλαγή στον αυτοματισμό:** επέλεξε πρώτα το
  `home_assistant/kolones_automation.yaml` και ξανατρέξε τη μετατροπή του §4
  (ή άλλαξε απευθείας το `.tpl`).
- **Νέα έκδοση:** αύξησε το `version` στο `config.yaml` και γράψε τι άλλαξε
  στο `CHANGELOG.md` — οι χρήστες θα δουν «Update» στον Store.
- **Αυτόματα builds:** μπορείς να προσθέσεις GitHub Actions με το
  `home-assistant/builder` για lint/build σε κάθε push.

---

## 7. Χρήσιμοι σύνδεσμοι

- [Home Assistant Add-on documentation](https://developers.home-assistant.io/docs/add-ons)
- [Add-on configuration (config.yaml)](https://developers.home-assistant.io/docs/add-ons/configuration)
- [Add-on communication & ingress](https://developers.home-assistant.io/docs/add-ons/communication)
- [bashio](https://github.com/hassio-addons/bashio)
- [s6-overlay v3](https://github.com/just-containers/s6-overlay)


---

## 8. Επεκτάσεις στην v1.1.0

- **API proxy μέσω ingress:** ο nginx τώρα προωθεί τα `/api/*` στον HA core και βάζει αυτός το Supervisor token (το `ingress.conf` παράγεται στην εκκίνηση με sed από το `ingress.conf.tpl`). Το dashboard ανιχνεύει ότι τρέχει σε ingress (`/api/hassio_ingress/...` στο path) και στέλνει εκεί τα calls — τέλος η εξάρτηση από «Keep me logged in». Σε ingress mode το WebSocket κλείνει (`USE_WEBSOCKET = !INGRESS`) και μένει το polling 10s.
- **Watchdog:** τρίτο s6 service (longrun) που κάνει poll τα `select.colX_led_mode` και στέλνει persistent notification σε πτώση/ανάκαμψη κολώνας. Αν είναι off, κάνει `exec tail -f /dev/null` (το busybox δεν έχει σίγουρα `sleep infinity`).
- **InfluxDB package:** προαιρετικό, με `!secret influxdb_password` - φορτώνει μόνο με πλήρες restart.
- **Grafana provisioning:** το dashboard JSON είχε `${DS_INFLUXDB}` από export-for-sharing - το sed το αντικαθιστά με το uid του provisioned datasource. Το `__inputs` metadata μένει αλλά είναι αδρανές εκτός import UI. Το Grafana add-on κάνει map το `/share` και δέχεται `GF_PATHS_PROVISIONING` μέσω env_vars.
- **write_if_changed:** τα packages ξαναγράφονται (και γίνεται reload) μόνο όταν αλλάξει το περιεχόμενο - τέλος οι επαναλαμβανόμενες ειδοποιήσεις σε κάθε restart του add-on.
