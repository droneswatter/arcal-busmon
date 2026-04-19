# arcal-busmon Notes

## Updating The README Screenshot

With the UI and demo publisher running, refresh `images/busmon-ui.png` from
`arcal-busmon/` with:

```bash
UI_URL="${UI_URL:-http://$(hostname -I | awk '{print $1}'):8765}" \
node -e "const { chromium } = require('playwright'); (async () => { const browser = await chromium.launch({ executablePath: process.env.HOME + '/.cache/ms-playwright/chromium-1208/chrome-linux64/chrome', args: ['--no-sandbox'] }); const page = await browser.newPage({ viewport: { width: 2048, height: 849 }, deviceScaleFactor: 1 }); await page.goto(process.env.UI_URL, { waitUntil: 'domcontentloaded' }); await page.waitForSelector('.row', { timeout: 10000 }); await page.waitForTimeout(800); await page.locator('.row').nth(3).click(); await page.waitForTimeout(500); await page.screenshot({ path: 'images/busmon-ui.png' }); await browser.close(); })().catch(err => { console.error(err); process.exit(1); });"
```

If the UI is running somewhere else, pass an explicit URL:

```bash
UI_URL=http://localhost:8765 node -e "..."
```
