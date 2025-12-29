# multi-thread-server

## Build

```
make
```

## Run

Start the single-threaded HTTP/1.0 server on port 8080:

```
./server
```

The server serves files from the current directory. Example test pages are included at the repository root.

## Verify (Phase 1)

Use curl to confirm responses:

```
curl -v http://localhost:8080/small.html
curl -v http://localhost:8080/does_not_exist.html
curl -v http://localhost:8080/large.html -o /dev/null
```
