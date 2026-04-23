# arcal-busmon Notes

## Updating The README Screenshot

With `arlacal-server`, the demo publisher, and the static page running, refresh
`images/busmon-ui.png` from `arcal-busmon/` with:

```bash
UI_URL="${UI_URL:-file://$PWD/ui/static/index.html?ws=ws://127.0.0.1:8766}" \
node -e "const { chromium } = require('playwright'); (async () => { const browser = await chromium.launch({ executablePath: process.env.HOME + '/.cache/ms-playwright/chromium-1208/chrome-linux64/chrome', args: ['--no-sandbox'] }); const page = await browser.newPage({ viewport: { width: 2048, height: 849 }, deviceScaleFactor: 1 }); await page.goto(process.env.UI_URL, { waitUntil: 'domcontentloaded' }); await page.waitForSelector('.row', { timeout: 10000 }); await page.waitForTimeout(800); await page.locator('.row').nth(3).click(); await page.waitForTimeout(500); await page.screenshot({ path: 'images/busmon-ui.png' }); await browser.close(); })().catch(err => { console.error(err); process.exit(1); });"
```

If the UI is running somewhere else, pass an explicit URL:

```bash
UI_URL='file:///path/to/arcal-busmon/ui/static/index.html?ws=ws://host:8766' node -e "..."
```
