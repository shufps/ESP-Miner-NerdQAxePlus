// Execute the production component with real Angular forms, not a copied regex.
// node influx-url-form.test.mjs [source-root], after npm ci
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import ts from 'typescript';
import '@angular/compiler';
import { FormBuilder } from '@angular/forms';
import * as forms from '@angular/forms';
import * as rxjs from 'rxjs';

const ui = path.dirname(fileURLToPath(import.meta.url));
const root = process.argv[2] || path.resolve(ui, '../../..');
const source = fs.readFileSync(path.join(root, 'main/http_server/axe-os/src/app/pages/influxdb/influxdb.component.ts'), 'utf8');
const compiled = ts.transpileModule(source, {compilerOptions: {target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS, experimentalDecorators: true}}).outputText;
const exports = {};
const imports = {
  '@angular/core': {Component: () => x => x, Input: () => () => {}},
  '@angular/forms': forms,
  'rxjs': rxjs,
};
vm.runInNewContext(compiled, {exports, require: name => {
  if (!(name in imports)) throw new Error(`Unmocked runtime import: ${name}`);
  return imports[name];
}});
const info = {url: 'http://192.168.0.123', port: 8086, bucket: 'telemetry', org: 'test', prefix: 'miner', enabled: 1};
let saved;
const service = {getInfluxInfo: () => rxjs.of(info), updateInflux: (_uri, payload) => { saved = payload; return rxjs.of({}); }};
const component = new exports.InfluxdbComponent(new FormBuilder(), service, {success() {}, danger() {}}, {lockUIUntilComplete: () => x => x}, {instant: x => x}, {ensureOtp$: () => rxjs.of({totp: ''})});
component.ngOnInit();
for (const url of ['http://192.168.0.123', 'http://influx.example.com', 'https://influx.example.com']) {
  component.form.get('url').setValue(url);
  assert.equal(component.form.valid, true, `expected valid: ${url}`);
  component.updateSystem();
  assert.equal(saved.url, url);
  assert.equal(saved.port, 8086);
  assert.equal('token' in saved, false, 'masked token must not overwrite the saved token');
}
for (const url of ['ftp://example.com', 'file://example.com', 'javascript:alert(1)', 'ws://example.com', 'example.com', 'https://example.com:443']) {
  component.form.get('url').setValue(url);
  assert.equal(component.form.invalid, true, `expected invalid: ${url}`);
}
const html = fs.readFileSync(path.join(root, 'main/http_server/axe-os/src/app/pages/influxdb/influxdb.component.html'), 'utf8');
assert.match(html, /\[disabled\]="[^"\n]*form\.invalid/);
console.log('PASS: production Influx component, real Angular FormBuilder, HTTP/HTTPS saves, invalid schemes/port denied');
