#pragma once
#include "generator/generator.h"

namespace Generator
{
	/**
	 * Generator for network replication traits, RPC stubs, and flatbuffers schemas.
	 *
	 * Scans REPLICATE(field, condition), RPC(method, target), and NET_CLASS(id)
	 * annotations on classes and fields.
	 *
	 * Produces (per-class):
	 *   1. _Generated/Network/ClassName.Replication.Gen.h  — ReplicationTraits<T>
	 *   2. _Generated/Network/ClassName.RPCStubs.Gen.h      — RPC send/register code
	 *
	 * Aggregate:
	 *   _Generated/Network/AllNetworkReplication.h
	 *   _Generated/Network/AllNetworkStubs.h
	 */
	class NetworkReplicationGenerator : public GeneratorInterface
	{
	public:
		NetworkReplicationGenerator() = delete;
		NetworkReplicationGenerator(std::string source_directory,
			std::function<std::string(std::string)> get_include_function);
		virtual int  generate(std::string path, SchemaMoudle schema) override;
		virtual void finish() override;
		virtual ~NetworkReplicationGenerator() override;

	protected:
		virtual void        prepareStatus(std::string path) override;
		virtual std::string processFileName(std::string path) override;

	private:
		// Collected per-class data
		struct ReplicaField
		{
			std::string field_name;
			std::string condition;   // "OnChange" | "Always"
			bool        owner_only = false;
			UInt32      bit_index = 0;
		};

		struct RPCMethod
		{
			std::string method_name;
			std::string target;      // "Server" | "Client" | "Multicast"
			UInt16      rpc_id = 0;  // assigned sequentially
		};

		struct NetClassDef
		{
			std::string class_name;
			std::string qualified_name;
			UInt16      class_id = 0;
			UInt32      property_count = 0;
			std::vector<ReplicaField> replicated_fields;
			std::vector<RPCMethod>    rpc_methods;
		};

		std::vector<NetClassDef>  m_classes;
		std::vector<std::string>  m_replication_headers;
		std::vector<std::string>  m_rpc_stub_headers;
		UInt16 m_next_rpc_id = 1;
	};
} // namespace Generator
