/**
 * Unit tests for MikanClient HTTP base URL composition
 */

import { composeHttpBaseUrl } from '../MikanClient';

describe('composeHttpBaseUrl', () => {
  it('returns an empty string until an http port is known', () => {
    expect(composeHttpBaseUrl('localhost', 0)).toBe('');
  });

  it('defaults an empty host to 127.0.0.1', () => {
    expect(composeHttpBaseUrl('', 8081)).toBe('http://127.0.0.1:8081');
  });

  it('strips schemes, trailing slashes, and ports from the host', () => {
    expect(composeHttpBaseUrl('ws://192.168.1.5:8080/', 9000)).toBe('http://192.168.1.5:9000');
    expect(composeHttpBaseUrl('wss://example.com', 9000)).toBe('http://example.com:9000');
    expect(composeHttpBaseUrl('http://host:1234', 9000)).toBe('http://host:9000');
  });
});
