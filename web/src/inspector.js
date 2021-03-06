/**
 * @license
 *
 * This file Copyright (C) 2020 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 */

import { FileRow } from './file-row.js';
import { Formatter } from './formatter.js';
import { Torrent } from './torrent.js';
import {
  OutsideClickListener,
  Utils,
  createTabsContainer,
  makeUUID,
  setEditableContent,
  setTextContent,
} from './utils.js';

const peer_column_classes = [
  'encryption',
  'speed-up',
  'speed-down',
  'percent-done',
  'status',
  'peer-address',
  'peer-app-name',
];

export class Inspector extends EventTarget {
  constructor(controller) {
    super();

    this.updating = false;
    this.settings_dirty = false;
    this.closed = false;
    this.controller = controller;
    this.elements = this._create();
    this.current_page = this.elements.info.root;
    this.interval = setInterval(this._refreshTorrents.bind(this), 3000);
    this.name = 'inspector';
    this.selection_listener = (event_) => this._setTorrents(event_.selected);
    this.torrent_listener = () => this._updateCurrentPage();
    this.torrents = [];
    this.file_torrent = null;
    this.file_torrent_n = null;
    this.file_rows = null;
    this.outside = new OutsideClickListener(this.elements.root);
    this.outside.addEventListener('click', () => this.close());
    Object.seal(this);

    controller.addEventListener(
      'torrent-selection-changed',
      this.selection_listener
    );
    this._setTorrents(this.controller.getSelectedTorrents());

    const on_change = this._onTorrentSettingsChanged.bind(this);
    this.elements.settings.ratio_limit.addEventListener('change', on_change);
    this.elements.settings.ratio_mode.addEventListener('change', on_change);

    document.body.append(this.elements.root);
  }

  close() {
    if (!this.closed) {
      this.outside.stop();
      clearInterval(this.interval);
      this._setTorrents([]);
      this.elements.root.remove();
      this.controller.removeEventListener(
        'torrent-selection-changed',
        this.selection_listener
      );
      this.dispatchEvent(new Event('close'));
      for (const property of Object.keys(this)) {
        this[property] = null;
      }
      this.closed = true;
    }
  }

  static _createInfoPage() {
    const root = document.createElement('div');
    root.classList.add('inspector-info-page');
    const elements = { root };

    const append_section_title = (text) => {
      const label = document.createElement('div');
      label.textContent = text;
      label.classList.add('section-label');
      root.append(label);
    };

    const append_row = (text) => {
      const lhs = document.createElement('label');
      setTextContent(lhs, text);
      root.append(lhs);

      const rhs = document.createElement('label');
      root.append(rhs);
      return rhs;
    };

    append_section_title('Activity');
    let rows = [
      ['have', 'Have:'],
      ['availability', 'Availability:'],
      ['uploaded', 'Uploaded:'],
      ['downloaded', 'Downloaded:'],
      ['state', 'State:'],
      ['running_time', 'Running time:'],
      ['remaining_time', 'Remaining:'],
      ['last_activity', 'Last activity:'],
      ['error', 'Error:'],
    ];
    for (const [name, text] of rows) {
      elements[name] = append_row(text);
    }

    append_section_title('Details');
    rows = [
      ['size', 'Size:'],
      ['location', 'Location:'],
      ['hash', 'Hash:'],
      ['privacy', 'Privacy:'],
      ['origin', 'Origin:'],
      ['comment', 'Comment:'],
    ];
    for (const [name, text] of rows) {
      elements[name] = append_row(text);
    }

    return elements;
  }

  static _createListPage(list_type, list_id) {
    const root = document.createElement('div');
    const list = document.createElement(list_type);
    list.id = list_id;
    root.append(list);
    return { list, root };
  }

  static _createTiersPage() {
    return Inspector._createListPage('div', 'inspector-tiers-list');
  }

  static _createFilesPage() {
    return Inspector._createListPage('ul', 'inspector-file-list');
  }

  static _createPeersPage() {
    const table = document.createElement('table');
    table.classList.add('peer-list');
    const thead = document.createElement('thead');
    const tr = document.createElement('tr');
    const names = ['', 'Up', 'Down', 'Done', 'Status', 'Address', 'Client'];
    names.forEach((name, index) => {
      const th = document.createElement('th');
      const classname = peer_column_classes[index];
      if (classname === 'encryption') {
        th.dataset.encrypted = true;
      }
      th.classList.add(classname);
      setTextContent(th, name);
      tr.append(th);
    });
    const tbody = document.createElement('tbody');
    thead.append(tr);
    table.append(thead);
    table.append(tbody);
    return {
      root: table,
      tbody,
    };
  }

  static _createSettingsPage() {
    const root = document.createElement('div');
    root.classList.add('inspector-settings-page');

    let label = document.createElement('div');
    label.textContent = 'Torrent Settings';
    label.classList.add('section-label');
    root.append(label);

    label = document.createElement('label');
    label.textContent = 'Ratio Limit:';
    root.append(label);

    let input = document.createElement('input');
    input.type = 'number';
    input.step = '0.01';
    input.id = makeUUID();
    input.dataset.key = 'seedRatioLimit';
    label.setAttribute('for', input.id);
    root.append(input);
    const ratio_limit = input;

    label = document.createElement('label');
    label.textContent = 'Ratio Mode:';
    root.append(label);

    input = document.createElement('select');
    input.id = makeUUID();
    input.dataset.key = 'seedRatioMode';
    input.add(new Option('Use Global Limit', 0));
    input.add(new Option('Use Per-Torrent Limit', 1));
    input.add(new Option('Unlimited', 2));
    label.setAttribute('for', input.id);
    root.append(input);
    const ratio_mode = input;

    label = document.createElement('label');
    label.textContent = 'Ratio';

    return { ratio_limit, ratio_mode, root };
  }

  _create() {
    const pages = {
      files: Inspector._createFilesPage(),
      info: Inspector._createInfoPage(),
      peers: Inspector._createPeersPage(),
      settings: Inspector._createSettingsPage(),
      tiers: Inspector._createTiersPage(),
    };

    const on_activated = (page) => {
      this.current_page = page;
      this._updateCurrentPage();
    };

    const elements = createTabsContainer(
      'inspector',
      [
        ['inspector-tab-info', pages.info.root],
        ['inspector-tab-peers', pages.peers.root],
        ['inspector-tab-tiers', pages.tiers.root],
        ['inspector-tab-files', pages.files.root],
        ['inspector-tab-settings', pages.settings.root],
      ],
      on_activated.bind(this)
    );

    return { ...elements, ...pages };
  }

  _setTorrents(torrents) {
    // update the inspector when a selected torrent's data changes.
    const key = 'dataChanged';
    const callback = this.torrent_listener;
    this.torrents.forEach((t) => t.removeEventListener(key, callback));
    this.torrents = [...torrents];
    this.torrents.forEach((t) => t.addEventListener(key, callback));

    this._refreshTorrents();
    this._updateCurrentPage();
  }

  static _needsExtraInfo(torrents) {
    return torrents.some((tor) => !tor.hasExtraInfo());
  }

  _refreshTorrents() {
    const { controller, torrents } = this;
    const ids = torrents.map((t) => t.getId());

    if (ids && ids.length > 0) {
      const fields = ['id', ...Torrent.Fields.StatsExtra];
      if (Inspector._needsExtraInfo(torrents)) {
        fields.push(...Torrent.Fields.InfoExtra);
      }
      if (this.settings_dirty) {
        this.settings_dirty = false;
        fields.push(...Torrent.Fields.SettingsExtra);
      }

      controller.updateTorrents(ids, fields);
    }
  }

  _updateCurrentPage() {
    const { elements } = this;
    this.updating = true;
    switch (this.current_page) {
      case elements.files.root:
        this._updateFiles();
        break;
      case elements.info.root:
        this._updateInfo();
        break;
      case elements.peers.root:
        this._updatePeers();
        break;
      case elements.tiers.root:
        this._updateTiers();
        break;
      case elements.settings.root:
        this._updateSettings();
        break;
      default:
        console.warn('unexpected page');
        console.log(this.current_page);
    }
    this.updating = false;
  }

  _updateInfo() {
    const none = 'None';
    const mixed = 'Mixed';
    const unknown = 'Unknown';
    const fmt = Formatter;
    const now = Date.now();
    const { torrents } = this;
    const e = this.elements;
    const sizeWhenDone = torrents.reduce(
      (accumulator, t) => accumulator + t.getSizeWhenDone(),
      0
    );

    // state
    let string = null;
    if (torrents.length === 0) {
      string = none;
    } else if (torrents.every((t) => t.isFinished())) {
      string = 'Finished';
    } else if (torrents.every((t) => t.isStopped())) {
      string = 'Paused';
    } else {
      const get = (t) => t.getStateString();
      const first = get(torrents[0]);
      string = torrents.every((t) => get(t) === first) ? first : mixed;
    }
    setTextContent(e.info.state, string);
    const stateString = string;

    // have
    if (torrents.length === 0) {
      string = none;
    } else {
      const verified = torrents.reduce(
        (accumulator, t) => accumulator + t.getHaveValid(),
        0
      );
      const unverified = torrents.reduce(
        (accumulator, t) => accumulator + t.getHaveUnchecked(),
        0
      );
      const leftUntilDone = torrents.reduce(
        (accumulator, t) => accumulator + t.getLeftUntilDone(),
        0
      );

      const d =
        100 *
        (sizeWhenDone ? (sizeWhenDone - leftUntilDone) / sizeWhenDone : 1);
      string = fmt.percentString(d);

      if (!unverified && !leftUntilDone) {
        string = `${fmt.size(verified)} (100%)`;
      } else if (!unverified) {
        string = `${fmt.size(verified)} of ${fmt.size(
          sizeWhenDone
        )} (${string}%)`;
      } else {
        string = `${fmt.size(verified)} of ${fmt.size(
          sizeWhenDone
        )} (${string}%), ${fmt.size(unverified)} Unverified`;
      }
    }
    setTextContent(e.info.have, string);

    // availability
    if (torrents.length === 0) {
      string = none;
    } else if (sizeWhenDone === 0) {
      string = none;
    } else {
      const available = torrents.reduce(
        (accumulator, t) => t.getHave() + t.getDesiredAvailable(),
        0
      );
      string = `${fmt.percentString((100 * available) / sizeWhenDone)}%`;
    }
    setTextContent(e.info.availability, string);

    //  downloaded
    if (torrents.length === 0) {
      string = none;
    } else {
      const d = torrents.reduce(
        (accumulator, t) => accumulator + t.getDownloadedEver(),
        0
      );
      const f = torrents.reduce(
        (accumulator, t) => accumulator + t.getFailedEver(),
        0
      );
      string = f ? `${fmt.size(d)} (${fmt.size(f)} corrupt)` : fmt.size(d);
    }
    setTextContent(e.info.downloaded, string);

    // uploaded
    if (torrents.length === 0) {
      string = none;
    } else {
      const u = torrents.reduce(
        (accumulator, t) => accumulator + t.getUploadedEver(),
        0
      );
      const d =
        torrents.reduce(
          (accumulator, t) => accumulator + t.getDownloadedEver(),
          0
        ) ||
        torrents.reduce((accumulator, t) => accumulator + t.getHaveValid(), 0);
      string = `${fmt.size(u)} (Ratio: ${fmt.ratioString(Utils.ratio(u, d))})`;
    }
    setTextContent(e.info.uploaded, string);

    // running time
    if (torrents.length === 0) {
      string = none;
    } else if (torrents.every((t) => t.isStopped())) {
      string = stateString; // paused || finished}
    } else {
      const get = (t) => t.getStartDate();
      const first = get(torrents[0]);
      string = !torrents.every((t) => get(t) === first)
        ? mixed
        : fmt.timeInterval(now / 1000 - first);
    }
    setTextContent(e.info.running_time, string);

    // remaining time
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getETA();
      const first = get(torrents[0]);
      if (!torrents.every((t) => get(t) === first)) {
        string = mixed;
      } else if (first < 0) {
        string = unknown;
      } else {
        string = fmt.timeInterval(first);
      }
    }
    setTextContent(e.info.remaining_time, string);

    // last active at
    if (torrents.length === 0) {
      string = none;
    } else {
      const latest = torrents.reduce(
        (accumulator, t) => Math.max(accumulator, t.getLastActivity()),
        -1
      );
      const now_seconds = Math.floor(now / 1000);
      if (0 < latest && latest <= now_seconds) {
        const idle_secs = now_seconds - latest;
        string =
          idle_secs < 5 ? 'Active now' : `${fmt.timeInterval(idle_secs)} ago`;
      } else {
        string = none;
      }
    }
    setTextContent(e.info.last_activity, string);

    // error
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getErrorString();
      const first = get(torrents[0]);
      string = torrents.every((t) => get(t) === first) ? first : mixed;
    }
    setTextContent(e.info.error, string || none);

    // size
    if (torrents.length === 0) {
      string = none;
    } else {
      const size = torrents.reduce(
        (accumulator, t) => accumulator + t.getTotalSize(),
        0
      );
      if (!size) {
        string = 'None';
      } else {
        const get = (t) => t.getPieceSize();
        const pieceCount = torrents.reduce(
          (accumulator, t) => accumulator + t.getPieceCount(),
          0
        );
        const pieceString = fmt.number(pieceCount);
        const pieceSize = get(torrents[0]);
        string = torrents.every((t) => get(t) === pieceSize)
          ? `${fmt.size(size)} (${pieceString} pieces @ ${fmt.mem(pieceSize)})`
          : `${fmt.size(size)} (${pieceString} pieces)`;
      }
    }
    setTextContent(e.info.size, string);

    // hash
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getHashString();
      const first = get(torrents[0]);
      string = torrents.every((t) => get(t) === first) ? first : mixed;
    }
    setTextContent(e.info.hash, string);

    // privacy
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getPrivateFlag();
      const first = get(torrents[0]);
      if (!torrents.every((t) => get(t) === first)) {
        string = mixed;
      } else if (first) {
        string = 'Private to this tracker -- DHT and PEX disabled';
      } else {
        string = 'Public torrent';
      }
    }
    setTextContent(e.info.privacy, string);

    // comment
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getComment();
      const first = get(torrents[0]);
      string = torrents.every((t) => get(t) === first) ? first : mixed;
    }
    string = string || none;
    if (string.startsWith('https://') || string.startsWith('http://')) {
      string = encodeURI(string);
      Utils.setInnerHTML(
        e.info.comment,
        `<a href="${string}" target="_blank" >${string}</a>`
      );
    } else {
      setTextContent(e.info.comment, string);
    }

    // origin
    if (torrents.length === 0) {
      string = none;
    } else {
      let get = (t) => t.getCreator();
      const creator = get(torrents[0]);
      const mixed_creator = !torrents.every((t) => get(t) === creator);

      get = (t) => t.getDateCreated();
      const date = get(torrents[0]);
      const mixed_date = !torrents.every((t) => get(t) === date);

      const empty_creator = !creator || !creator.length;
      const empty_date = !date;
      if (mixed_creator || mixed_date) {
        string = mixed;
      } else if (empty_creator && empty_date) {
        string = unknown;
      } else if (empty_date && !empty_creator) {
        string = `Created by ${creator}`;
      } else if (empty_creator && !empty_date) {
        string = `Created on ${new Date(date * 1000).toDateString()}`;
      } else {
        string = `Created by ${creator} on ${new Date(
          date * 1000
        ).toDateString()}`;
      }
    }
    setTextContent(e.info.origin, string);

    // location
    if (torrents.length === 0) {
      string = none;
    } else {
      const get = (t) => t.getDownloadDir();
      const first = get(torrents[0]);
      string = torrents.every((t) => get(t) === first) ? first : mixed;
    }
    setTextContent(e.info.location, string);
  }

  ///  PEERS PAGE

  static _peerStatusTitle(flag_string) {
    const texts = Object.seal({
      '?': "We unchoked this peer, but they're not interested",
      D: 'Downloading from this peer',
      E: 'Encrypted Connection',
      H: 'Peer was discovered through Distributed Hash Table (DHT)',
      I: 'Peer is an incoming connection',
      K: "Peer has unchoked us, but we're not interested",
      O: 'Optimistic unchoke',
      T: 'Peer is connected via uTP',
      U: 'Uploading to peer',
      X: 'Peer was discovered through Peer Exchange (PEX)',
      d: "We would download from this peer if they'd let us",
      u: "We would upload to this peer if they'd ask",
    });

    return [...flag_string]
      .filter((ch) => texts[ch])
      .map((ch) => `${ch}: ${texts[ch]}`)
      .join('\n');
  }

  _updatePeers() {
    const fmt = Formatter;
    const { torrents } = this;
    const { tbody } = this.elements.peers;

    const cell_setters = [
      (peer, td) => {
        td.dataset.encrypted = peer.isEncrypted;
      },
      (peer, td) =>
        setTextContent(
          td,
          peer.rateToPeer ? fmt.speedBps(peer.rateToPeer) : ''
        ),
      (peer, td) =>
        setTextContent(
          td,
          peer.rateToClient ? fmt.speedBps(peer.rateToClient) : ''
        ),
      (peer, td) => setTextContent(td, `${Math.floor(peer.progress * 100)}%`),
      (peer, td) => {
        setTextContent(td, peer.flagStr);
        td.setAttribute('title', Inspector._peerStatusTitle(peer.flagStr));
      },
      (peer, td) => setTextContent(td, peer.address),
      (peer, td) => setTextContent(td, peer.clientName),
    ];

    const rows = [];
    for (const tor of torrents) {
      // torrent name
      const tortr = document.createElement('tr');
      tortr.classList.add('torrent-row');
      const tortd = document.createElement('td');
      tortd.setAttribute('colspan', cell_setters.length);
      setTextContent(tortd, tor.getName());
      tortr.append(tortd);
      rows.push(tortr);

      // peers
      for (const peer of tor.getPeers()) {
        const tr = document.createElement('tr');
        tr.classList.add('peer-row');
        cell_setters.forEach((setter, index) => {
          const td = document.createElement('td');
          td.classList.add(peer_column_classes[index]);
          setter(peer, td);
          tr.append(td);
        });
        rows.push(tr);
      }

      // TODO: modify instead of rebuilding wholesale?
      while (tbody.firstChild) {
        tbody.firstChild.remove();
      }
      tbody.append(...rows);
    }
  }

  /// TRACKERS PAGE

  static getAnnounceState(tracker) {
    switch (tracker.announceState) {
      case Torrent._TrackerActive:
        return 'Announce in progress';
      case Torrent._TrackerWaiting: {
        const timeUntilAnnounce = Math.max(
          0,
          tracker.nextAnnounceTime - new Date().getTime() / 1000
        );
        return `Next announce in ${Formatter.timeInterval(timeUntilAnnounce)}`;
      }
      case Torrent._TrackerQueued:
        return 'Announce is queued';
      case Torrent._TrackerInactive:
        return tracker.isBackup
          ? 'Tracker will be used as a backup'
          : 'Announce not scheduled';
      default:
        return `unknown announce state: ${tracker.announceState}`;
    }
  }

  static lastAnnounceStatus(tracker) {
    let lastAnnounceLabel = 'Last Announce';
    let lastAnnounce = ['N/A'];

    if (tracker.hasAnnounced) {
      const lastAnnounceTime = Formatter.timestamp(tracker.lastAnnounceTime);
      if (tracker.lastAnnounceSucceeded) {
        lastAnnounce = [
          lastAnnounceTime,
          ' (got ',
          Formatter.countString('peer', 'peers', tracker.lastAnnouncePeerCount),
          ')',
        ];
      } else {
        lastAnnounceLabel = 'Announce error';
        lastAnnounce = [
          tracker.lastAnnounceResult ? `${tracker.lastAnnounceResult} - ` : '',
          lastAnnounceTime,
        ];
      }
    }
    return {
      label: lastAnnounceLabel,
      value: lastAnnounce.join(''),
    };
  }

  static lastScrapeStatus(tracker) {
    let lastScrapeLabel = 'Last Scrape';
    let lastScrape = 'N/A';

    if (tracker.hasScraped) {
      const lastScrapeTime = Formatter.timestamp(tracker.lastScrapeTime);
      if (tracker.lastScrapeSucceeded) {
        lastScrape = lastScrapeTime;
      } else {
        lastScrapeLabel = 'Scrape error';
        lastScrape =
          (tracker.lastScrapeResult ? `${tracker.lastScrapeResult} - ` : '') +
          lastScrapeTime;
      }
    }
    return {
      label: lastScrapeLabel,
      value: lastScrape,
    };
  }

  _updateTiers() {
    const na = 'N/A';
    const { list } = this.elements.tiers;
    const { torrents } = this;

    const rows = [];
    for (const tor of torrents) {
      const group = document.createElement('div');
      group.classList.add('inspector-group');
      rows.push(group);

      // if >1 torrent to be shown, give a title
      if (torrents.length > 1) {
        const title = document.createElement('div');
        title.classList.add('tier-list-torrent');
        setTextContent(title, tor.getName());
        rows.push(title);
      }

      tor.getTrackers().forEach((tracker, index) => {
        const announceState = Inspector.getAnnounceState(tracker);
        const lastAnnounceStatusHash = Inspector.lastAnnounceStatus(tracker);
        const lastScrapeStatusHash = Inspector.lastScrapeStatus(tracker);

        const tier_div = document.createElement('div');
        tier_div.classList.add('tier-list-row', index % 2 ? 'odd' : 'even');

        let element = document.createElement('div');
        element.classList.add('tier-list-tracker');
        setTextContent(
          element,
          `${tracker.domain || tracker.host || tracker.announce} - tier ${
            tracker.tier + 1
          }`
        );
        element.setAttribute('title', tracker.announce);
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-announce');
        setTextContent(
          element,
          `${lastAnnounceStatusHash.label}: ${lastAnnounceStatusHash.value}`
        );
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-seeders');
        setTextContent(
          element,
          `Seeders: ${tracker.seederCount > -1 ? tracker.seederCount : na}`
        );
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-state');
        setTextContent(element, announceState);
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-leechers');
        setTextContent(
          element,
          `Leechers: ${tracker.leecherCount > -1 ? tracker.leecherCount : na}`
        );
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-scrape');
        setTextContent(
          element,
          `${lastScrapeStatusHash.label}: ${lastScrapeStatusHash.value}`
        );
        tier_div.append(element);

        element = document.createElement('div');
        element.classList.add('tier-downloads');
        setTextContent(
          element,
          `Downloads: ${
            tracker.downloadCount > -1 ? tracker.downloadCount : na
          }`
        );
        tier_div.append(element);

        rows.push(tier_div);
      });
    }

    // TODO: modify instead of rebuilding wholesale?
    while (list.firstChild) {
      list.firstChild.remove();
    }
    list.append(...rows);
  }

  ///  SETTINGS PAGE

  _onTorrentSettingsChanged(event_) {
    if (this.updating) {
      return;
    }

    const { key } = event_.target.dataset;
    const { torrents, controller } = this;
    const val = Inspector._getValue(event_.target);

    if (val === '') {
      return;
    }

    event_.target.lastvalue = val;

    const args = { [key]: val };
    controller.editTorrents(torrents, args);
    this.settings_dirty = true;
  }

  static _getValue(e) {
    switch (e.type) {
      case 'select-one':
      case 'select-multiple':
      case 'number':
      case 'text':
      case 'url': {
        const string = e.value;
        const n = Number(string);
        if (!Number.isNaN(n)) {
          return n;
        }
        return string;
      }
      default:
        return null;
    }
  }

  _updateSettings() {
    const { torrents, elements } = this;
    let limit = '';
    let mode = '';

    const get_common_value_for_field = (field) => {
      if (
        torrents.every((v) => v.fields[field] === torrents[0].fields[field]) &&
        torrents.length > 0
      ) {
        return torrents[0].fields[field];
      }
      return '';
    };

    limit = get_common_value_for_field('seedRatioLimit');
    mode = get_common_value_for_field('seedRatioMode');

    setEditableContent(elements.settings.ratio_limit, limit);
    setEditableContent(elements.settings.ratio_mode, mode);
  }

  ///  FILES PAGE

  _changeFileCommand(fileIndices, command) {
    const { controller, file_torrent } = this;
    const torrentId = file_torrent.getId();
    controller.changeFileCommand(torrentId, fileIndices, command);
  }

  _onFileWantedToggled(event_) {
    const { indices, wanted } = event_;
    this._changeFileCommand(
      indices,
      wanted ? 'files-wanted' : 'files-unwanted'
    );
  }

  _onFilePriorityToggled(event_) {
    const { indices, priority } = event_;

    let command = null;
    switch (priority) {
      case -1:
        command = 'priority-low';
        break;
      case 1:
        command = 'priority-high';
        break;
      default:
        command = 'priority-normal';
        break;
    }

    this._changeFileCommand(indices, command);
  }

  _clearFileList() {
    const { list } = this.elements.files;
    while (list.firstChild) {
      list.firstChild.remove();
    }

    this.file_torrent = null;
    this.file_torrent_n = null;
    this.file_rows = null;
  }

  static createFileTreeModel(tor) {
    const leaves = [];
    const tree = {
      children: {},
      file_indices: [],
    };

    tor.getFiles().forEach((file, index) => {
      const { name } = file;
      const tokens = name.split('/');
      let walk = tree;
      for (const [index_, token] of tokens.entries()) {
        let sub = walk.children[token];
        if (!sub) {
          walk.children[token] = sub = {
            children: {},
            depth: index_,
            file_indices: [],
            name: token,
            parent: walk,
          };
        }
        walk = sub;
      }
      walk.file_index = index;
      delete walk.children;
      leaves.push(walk);
    });

    for (const leaf of leaves) {
      const { file_index } = leaf;
      let walk = leaf;
      do {
        walk.file_indices.push(file_index);
        walk = walk.parent;
      } while (walk);
    }

    return tree;
  }

  addNodeToView(tor, parent, sub, index) {
    const row = new FileRow(
      tor,
      sub.depth,
      sub.name,
      sub.file_indices,
      index % 2
    );
    row.addEventListener('wantedToggled', this._onFileWantedToggled.bind(this));
    row.addEventListener(
      'priorityToggled',
      this._onFilePriorityToggled.bind(this)
    );
    this.file_rows.push(row);
    parent.append(row.getElement());
  }

  addSubtreeToView(tor, parent, sub, index) {
    if (sub.parent) {
      this.addNodeToView(tor, parent, sub, index++);
    }
    if (sub.children) {
      for (const value of Object.values(sub.children)) {
        index = this.addSubtreeToView(tor, parent, value, index);
      }
    }
    return index;
  }

  _updateFiles() {
    const { list } = this.elements.files;
    const { torrents } = this;

    // only show one torrent at a time
    if (torrents.length !== 1) {
      this._clearFileList();
      return;
    }

    const [tor] = torrents;
    const n = tor.getFiles().length;
    if (tor !== this.file_torrent || n !== this.file_torrent_n) {
      // rebuild the file list...
      this._clearFileList();
      this.file_torrent = tor;
      this.file_torrent_n = n;
      this.file_rows = [];
      const fragment = document.createDocumentFragment();
      const tree = Inspector.createFileTreeModel(tor);
      this.addSubtreeToView(tor, fragment, tree, 0);
      list.append(fragment);
    } else {
      // ...refresh the already-existing file list
      this.file_rows.forEach((row) => row.refresh());
    }
  }
}
