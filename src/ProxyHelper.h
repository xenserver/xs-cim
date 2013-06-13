CMPIrc prov_pxy_init();
CMPIrc prov_pxy_uninit();

CMPIrc prov_pxy_begin(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const char *classname, 
    void *ctx, 
    bool refs_only,
    const char **properties,
    void **res_list
    );

CMPIrc prov_pxy_get(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context * caller_id, 
    const char **properties,
    void **res
    );

CMPIrc prov_pxy_getnext(
    const XenProviderInstanceFT* ft,
    void *res_list, 
    const char **properties,
    void **res
    );

void prov_pxy_end(
    const XenProviderInstanceFT* ft,
    void *res_list);

CMPIrc prov_pxy_add(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context *caller_id, 
    const void *res
    );

CMPIrc prov_pxy_delete(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context *caller_id
    );

CMPIrc prov_pxy_extract(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const CMPIInstance *inst,
    const char **properties,
    void **res
    );

CMPIrc prov_pxy_extractid(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const CMPIInstance* inst,
    void **res_id
    );

void prov_pxy_release(
    const XenProviderInstanceFT* ft,
    void *res);

void prov_pxy_releaseid(
    const XenProviderInstanceFT* ft,
    void* res_id
    );

CMPIrc prov_pxy_setproperties(
    const XenProviderInstanceFT* ft,
    CMPIInstance *inst, 
    const void *res,
    const char **properties);

CMPIrc prov_pxy_modify(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id,
    struct xen_call_context *caller_id, 
    const void *modified_res,
    const char **properties);
