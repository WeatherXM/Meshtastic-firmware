// Regression coverage for the embedded WG1200 page using the node API contract.
// The old page expected a nested schema and silently displayed no readings. These
// browser checks also protect selection from TFT/poll changes, missing-vs-zero
// measurements, unknown cached timestamps, rotation, and failed refreshes.
// Run with Playwright and Chrome available: node --test test/browser/weather_dashboard.test.cjs
const { test } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { chromium } = require("playwright");

const source = fs.readFileSync(
  path.join(__dirname, "../../src/mesh/http/WeatherDashboard.h"),
  "utf8",
);
const html = source.split('R"rawliteral(')[1].split(')rawliteral";')[0];
const fixtures = [
  {
    node_id: "!12345678",
    name: 'Garden "north"',
    age_seconds: 12,
    metrics: {
      temperature: 20,
      relative_humidity: 0,
      barometric_pressure: 1000,
      wind_speed: 0,
      rainfall_24h: 2.5,
    },
  },
  {
    node_id: "!87654321",
    name: 'Roof <img src=x onerror="window.injected=true">',
    age_seconds: null,
    metrics: { temperature: -5, lux: 120, voltage: 3.3 },
  },
];

test("weather node dashboard interactions and API failure recovery", async (t) => {
  const browser = await chromium.launch({
    headless: true,
    channel: process.env.PLAYWRIGHT_CHANNEL || "chrome",
  });
  t.after(() => browser.close());
  const page = await browser.newPage({
    viewport: { width: 1200, height: 900 },
  });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  let payload = structuredClone(fixtures),
    fail = false;
  await page.route("http://weather.test/**", async (route) => {
    if (route.request().url().endsWith("/api/v1/weather/nodes")) {
      await route.fulfill({
        status: fail ? 503 : 200,
        contentType: "application/json",
        body: JSON.stringify({ nodes: payload }),
      });
    } else await route.fulfill({ contentType: "text/html", body: html });
  });
  await page.clock.install();
  await page.goto("http://weather.test/weather");
  await page.locator(".metric").first().waitFor();
  assert.equal(await page.locator(".node").count(), 2);
  assert.match(
    await page.locator('[data-metric="temperature"]').innerText(),
    /20\.0/,
  );
  assert.match(
    await page.locator('[data-metric="relative_humidity"]').innerText(),
    /0/,
  );
  assert.equal(await page.locator('[data-metric="wind_gust"]').count(), 0);
  assert.match(await page.locator("#updated").innerText(), /Updated 12s ago/);

  await page.locator(".node").nth(1).click();
  assert.equal(await page.locator("#name").textContent(), fixtures[1].name);
  assert.equal(await page.locator("#name img").count(), 0);
  assert.equal(await page.evaluate(() => window.injected), undefined);
  assert.match(
    await page.locator("#updated").innerText(),
    /update time unknown/,
  );
  assert.equal(
    await page.locator('[data-metric="relative_humidity"]').count(),
    0,
  );
  await page.locator("#refresh").click();
  await page.waitForFunction(
    () => !document.getElementById("refresh").disabled,
  );
  assert.equal(
    await page.locator("#node-id").textContent(),
    fixtures[1].node_id,
  );
  await page.locator("#units").click();
  assert.match(
    await page.locator('[data-metric="temperature"]').innerText(),
    /23\.0\s*°F/,
  );

  await page.locator("#rotate").click();
  await page.clock.fastForward(10001);
  assert.equal(
    await page.locator("#node-id").textContent(),
    fixtures[0].node_id,
  );
  await page.locator(".node").nth(1).click();
  assert.equal(
    await page.locator("#rotate").getAttribute("aria-pressed"),
    "false",
  );
  await page.clock.fastForward(10001);
  assert.equal(
    await page.locator("#node-id").textContent(),
    fixtures[1].node_id,
  );

  fail = true;
  await page.locator("#refresh").click();
  await page.waitForFunction(
    () => document.getElementById("connection").dataset.ok === "false",
  );
  assert.match(
    await page.locator("#connection").innerText(),
    /showing saved readings/,
  );
  assert.equal(
    await page.locator("#node-id").textContent(),
    fixtures[1].node_id,
  );
  fail = false;
  payload = [fixtures[0]];
  await page.locator("#refresh").click();
  await page.waitForFunction(
    () => document.querySelectorAll(".node").length === 1,
  );
  assert.equal(await page.locator("#rotate").isDisabled(), true);
  assert.equal(
    await page.locator("#node-id").textContent(),
    fixtures[0].node_id,
  );

  payload = fixtures;
  await page.locator("#refresh").click();
  await page.waitForFunction(
    () => document.querySelectorAll(".node").length === 2,
  );
  await page.setViewportSize({ width: 390, height: 844 });
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= window.innerWidth,
    ),
    true,
  );
  if (process.env.WEATHER_SCREENSHOT)
    await page.screenshot({
      path: process.env.WEATHER_SCREENSHOT,
      fullPage: true,
    });

  payload = [];
  await page.locator("#refresh").click();
  await page.waitForFunction(() => document.getElementById("detail").hidden);
  assert.match(
    await page.locator("#empty").innerText(),
    /No weather nodes yet/,
  );
  assert.equal(await page.locator("#rotate").isDisabled(), true);
  assert.deepEqual(errors, []);
});
