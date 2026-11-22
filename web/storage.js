// Mount the existing highscore directory before main can load either character's table.
(() => {
    const mount = '/gravity-leon';
    let persistent = false;
    let dirty = false;
    let writing = null;

    function sync(populate) {
        return new Promise((resolve, reject) => {
            FS.syncfs(populate, error => error ? reject(error) : resolve());
        });
    }
    // Called after the asset pack has populated /data and before Play becomes available.
    Module.prepareScores = async () => {
        const shipped = '/shipped-data';
        let mounted = false;
        try {
            if (!globalThis.indexedDB) throw new Error('IndexedDB unavailable');
            FS.mkdir(mount);
            FS.mount(IDBFS, {}, mount);
            mounted = true;
            await sync(true);
            for (const name of FS.readdir('/data')) {
                if (name === '.' || name === '..') continue;
                const saved = mount + '/' + name;
                // Each release supplies its own tuning. Saved player tables take precedence over their defaults.
                if (name === 'params.txt' || !FS.analyzePath(saved).exists) {
                    FS.writeFile(saved, FS.readFile('/data/' + name));
                }
            }
            FS.rename('/data', shipped);
            FS.symlink(mount, '/data');
            persistent = true;
        } catch (error) {
            console.warn('Highscore storage unavailable', error);
            if (mounted) FS.unmount(mount);
            if (FS.analyzePath(shipped).exists) FS.rename(shipped, '/data');
            GravityLeon.message('Highscores are available for this session only.');
        }
    };

    // A save during a write must be included in a subsequent sync.
    async function drain() {
        try {
            while (dirty) {
                dirty = false;
                await sync(false);
            }
        } catch (error) {
            dirty = true;
            console.warn('Highscore write failed', error);
            GravityLeon.message('Highscores could not be saved to this browser.');
        } finally {
            writing = null;
        }
    }
    Module.flushScores = () => {
        if (persistent && dirty && !writing) writing = drain();
        return writing;
    };
    Module.scoresChanged = () => {
        dirty = true;
        Module.flushScores();
    };
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) Module.flushScores();
    });
})();
