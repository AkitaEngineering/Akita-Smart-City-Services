# ASCS Management Console

The console displays validated live MQTT telemetry plus up to 500 Firestore history records. Firebase Authentication requires an `operator` or `admin` custom claim. Infrastructure registration is persisted in Firestore. Commands are audited as pending, published with MQTT QoS 1, and are not reported as successful until the target node returns a matching execution acknowledgement.

```bash
cp .env.example .env
npm ci
npm run check
npm run dev
```

Required public configuration is listed in `.env.example`. Production builds require `wss://`. After login, the console uses the operator's short-lived Firebase ID token as the MQTT password and the Firebase UID as the MQTT username. Configure the broker to validate Firebase JWTs and apply the minimum topic ACL described in the root README. Never put a reusable broker password in a `VITE_*` variable: Vite variables are public bundle data. There is no simulator or production fallback data.

The Vite production build fails when any required setting is absent, the MQTT topic is unsafe, or the broker URL is not credential-free `wss://`.

Deploy from the repository root after installing Firebase CLI and authenticating to the intended project:

```bash
firebase use <project-id>
firebase deploy --only firestore:rules,hosting
```

Before granting access, set the Firebase custom claim `role` to `operator` or `admin` using a trusted Admin SDK process. Never let a client assign its own role.
