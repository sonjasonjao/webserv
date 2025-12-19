# Train of thought

## In the beginning
- There is a **request**
- The request is associated with a **socket fd**, that socket fd is associated with a **port**
- The request might contain a **host header**
    - Try matching the **host name** and **port** to a **config**
    - If a matching config can't be found use the **default config** for that port,
    which is the first one containing that port

- There might be a **URI** in the request
- The **config** might have one or more **routes**
    - Loop over all config reroutes and see if the URI contains such a route
    - Replace part of the URI with the correct route
    - Make sure that the **request type is allowed** for that route in the config
- Make sure the **resource exists**
    - If the resource can't be found in the default locations/rerouted folders:
        - **Q:** Do we search the upload folder?
        - **Q:** Should we disable uploads to subfolders of the upload folder?
        - **Q:** Do we validate against upload folder conflicts, or if two 'servers' upload to the
        same folder do we care about file collisions?

## Requested resource is text
- If the resource isn't too large, retrieve the resource
    - From memory, or load into memory
    - If the resource is super large, consider **chunking**
        - Read part of the resource into memory and send it
        - Store how much of the resource has been sent, keep sending chunks until complete

## Requested resource is not text
- Must be inferred from file extension
- **Q:** Do we use htons/htonl?
- **Q:** Can non text files be cached in strings?
- **Q:** If the resource is an image, do we have an image folder in the config, or a shared default?
    - Look in upload folder if resource can't be found elsewhere

## Requested resource is a folder
- The config can enable **autoindexing** or **directory listing**

## Errors
- In case of an **error** retrieve and send **matching error page**
    - Config can list error pages
        - **Q:** Does the config only specify a directory, and error files have a naming scheme, or explicit filepaths per error?
        - **Q:** If error page indicated by the config doesn't exist or there is some other error trying
        to access that page, send server error, or the default error page that matches the request?

## Response queue
- Outgoing responses should be handled in a loop
