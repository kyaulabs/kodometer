pragma ComponentBehavior: Bound

import QtQuick
import plasma.applet.org.kyaulabs.kodometer as Private

QtObject {
    id: root

    property var providers: []
    property var catalog: []
    property string requestedProviderId: ""
    readonly property var displayedProviders: {
        const rows = providers.slice()
        const requested = catalog.find(row => row.id === requestedProviderId)
        if (requested && !rows.some(row => row.id === requestedProviderId))
            rows.push({
                          id: requested.id,
                          name: requested.name,
                          pendingSelection: true
                      })
        return rows
    }
    readonly property var selectedProvider: selection.selectedProvider
    readonly property string selectedProviderId: selection.selectedProviderId
    readonly property int selectedTabIndex: selection.selectedTabIndex
    readonly property bool overviewSelected: selection.overviewSelected

    readonly property Private.ProviderSelectionModel selection: Private.ProviderSelectionModel {
        providers: root.displayedProviders
        onProvidersChanged: root.restoreRequested()
    }

    onCatalogChanged: {
        if (!catalog.some(row => row.id === requestedProviderId))
            requestedProviderId = ""
    }

    function restoreRequested() {
        Qt.callLater(function () {
            if (root.requestedProviderId.length > 0)
                root.selection.selectProvider(root.requestedProviderId)
        })
    }
    function focusProvider(providerId) {
        if (!catalog.some(row => row.id === providerId))
            return
        requestedProviderId = providerId
        restoreRequested()
    }
    function selectOverview() {
        requestedProviderId = ""
        selection.selectOverview()
    }
    function selectProvider(providerId) {
        requestedProviderId = ""
        selection.selectProvider(providerId)
    }
    function selectNext() {
        requestedProviderId = ""
        selection.selectNext()
    }
    function selectPrevious() {
        requestedProviderId = ""
        selection.selectPrevious()
    }
}
