# Siege

## Installation
```
brew install siege
```

## Fixing the bombardment script
```
vim $HOME/.brew/bin/bombardment
```
go to line 49 and replace the whole block with:
```
total=$((startcl + inc * numruns))
```

## Simplest cases
```
siege http://localhost:8081
```
`CTRL + C` to stop

To use the bombardment script you have to create a file with URLs, the simplest can just include:
```
http://localhost:8081
```
and then run:
```
bombardment bombardment_urls.txt 200 0 1 0.001
```
Check `--help` for more in depth info about the parameters.

## Changing the HTTP version
```
vim $HOME/.siege/siege.conf
```
-> line 213
